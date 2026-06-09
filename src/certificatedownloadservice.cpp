#include "certificatedownloadservice.h"

#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrlQuery>

namespace {
constexpr auto kLoginUrl = "https://newcert.iitd.ac.in/cgi-bin/usermanage/vpn3cert.cgi";
constexpr auto kDownloadUrl = "https://newcert.iitd.ac.in/cgi-bin/usermanage/getvpn3cert.cgi";

bool isConnectivityFailure(QNetworkReply::NetworkError error)
{
    switch (error) {
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::UnknownNetworkError:
        return true;
    default:
        return false;
    }
}

bool containsInsensitive(const QString &html, const char *needle)
{
    return html.contains(QString::fromUtf8(needle), Qt::CaseInsensitive);
}
}

CertificateDownloadService::CertificateDownloadService(QObject *parent)
    : QObject(parent)
    , settings("IIT Delhi", "IITDelhiVPN")
{
    certManager = new QNetworkAccessManager(this);
    certManager->setCookieJar(new QNetworkCookieJar(certManager));
}

QString CertificateDownloadService::downloadedOvpnPath() const
{
    return downloadedOvpnPathValue;
}

void CertificateDownloadService::loadSavedCertificateState()
{
    certificateDownloaded = settings.value("certificate/downloaded", false).toBool();
    downloadedOvpnPathValue = settings.value("certificate/path").toString();
    certUser = settings.value("certificate/user").toString();

    if (certificateDownloaded && !downloadedOvpnPathValue.isEmpty() && QFile::exists(downloadedOvpnPathValue)) {
        emit savedCertificateAvailable(downloadedOvpnPathValue);
        return;
    }

    clearSavedCertificateState();
    emit savedCertificateUnavailable();
}

void CertificateDownloadService::startCertificateDownload(const QString &username, const QString &password)
{
    certUser = username.trimmed();
    saveCertificateState();

    if (certUser.isEmpty() || password.isEmpty()) {
        emit warningOccurred("Missing details", "Enter both username and password.");
        return;
    }

    const QString targetPath = makeDownloadPath(certUser);
    if (targetPath.isEmpty()) {
        emit criticalOccurred("Download error", "Could not prepare the download folder.");
        return;
    }

    downloadedOvpnPathValue = targetPath;

    if (certificateDownloaded && QFile::exists(downloadedOvpnPathValue)) {
        emit informationOccurred("Certificate ready", QStringLiteral("Using the existing OVPN file:\n%1").arg(downloadedOvpnPathValue));
        emit savedCertificateAvailable(downloadedOvpnPathValue);
        return;
    }

    emit busyChanged(true);
    emit statusMessage(QStringLiteral("Downloading certificate..."));

    QUrlQuery query;
    query.addQueryItem("user", certUser);
    query.addQueryItem("passwd", password);

    connect(certManager, &QNetworkAccessManager::finished,
            this, &CertificateDownloadService::handleInitialResponse,
            Qt::SingleShotConnection);

    sendCertificateRequest(QUrl(kLoginUrl), query.toString(QUrl::FullyEncoded).toUtf8());
}

void CertificateDownloadService::saveCertificateState()
{
    settings.setValue("certificate/downloaded", certificateDownloaded);
    settings.setValue("certificate/path", downloadedOvpnPathValue);
    settings.setValue("certificate/user", certUser);
    settings.sync();
}

void CertificateDownloadService::clearSavedCertificateState()
{
    certificateDownloaded = false;
    downloadedOvpnPathValue.clear();
    certUser.clear();
    settings.remove("certificate/downloaded");
    settings.remove("certificate/path");
    settings.remove("certificate/user");
}

QString CertificateDownloadService::extractHiddenField(const QString &html, const QString &name) const
{
    const QRegularExpression regex(
        QStringLiteral(R"(<input[^>]*name\s*=\s*["']?%1["']?[^>]*value\s*=\s*["']?([^"'>\s]+))")
            .arg(QRegularExpression::escape(name)),
        QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch match = regex.match(html);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString CertificateDownloadService::makeDownloadPath(const QString &username) const
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (basePath.isEmpty()) {
        basePath = QDir::homePath();
    }

    QDir dir(basePath);
    if (!dir.mkpath("IITDelhiVPN")) {
        return {};
    }

    const QString safeName = username.trimmed().isEmpty() ? QStringLiteral("iitdelhi-vpn") : username.trimmed();
    return dir.filePath(QStringLiteral("IITDelhiVPN/%1.ovpn").arg(safeName));
}

void CertificateDownloadService::sendCertificateRequest(const QUrl &url, const QByteArray &body)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    certManager->post(request, body);
}

void CertificateDownloadService::handleInitialResponse(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit busyChanged(false);
        if (isConnectivityFailure(reply->error())) {
            emit warningOccurred("No internet", "Internet is switched off. Please turn it on and try again.");
        } else {
            emit criticalOccurred("Download error", reply->errorString());
        }
        return;
    }

    const QString html = QString::fromUtf8(reply->readAll());

    if (containsInsensitive(html, "Unauthorized access, incorrect password or invalid session")) {
        emit busyChanged(false);
        emit warningOccurred("Login failed", "Incorrect username or password was entered.");
        return;
    }

    certSession = extractHiddenField(html, "session");
    certControl = extractHiddenField(html, "control");
    certEmail = extractHiddenField(html, "email");
    certCategory = extractHiddenField(html, "category");

    if (certSession.isEmpty() || certControl.isEmpty() || certEmail.isEmpty() || certCategory.isEmpty()) {
        emit busyChanged(false);
        emit criticalOccurred("Download error", "The certificate page did not return the expected session details.");
        return;
    }

    QUrlQuery query;
    query.addQueryItem("user", certUser);
    query.addQueryItem("session", certSession);
    query.addQueryItem("control", certControl);
    query.addQueryItem("email", certEmail);
    query.addQueryItem("category", certCategory);
    query.addQueryItem("ovpn", "Download OVPN Configuration");

    connect(certManager, &QNetworkAccessManager::finished,
            this, &CertificateDownloadService::handleDownloadResponse,
            Qt::SingleShotConnection);

    sendCertificateRequest(QUrl(kDownloadUrl), query.toString(QUrl::FullyEncoded).toUtf8());
}

void CertificateDownloadService::handleDownloadResponse(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit busyChanged(false);
        if (isConnectivityFailure(reply->error())) {
            emit warningOccurred("No internet", "Internet is switched off. Please turn it on and try again.");
        } else {
            emit criticalOccurred("Download error", reply->errorString());
        }
        return;
    }

    const QByteArray ovpnData = reply->readAll();
    const QString responseText = QString::fromUtf8(ovpnData);

    if (containsInsensitive(responseText, "Your session is invalid/expired")) {
        emit busyChanged(false);
        emit warningOccurred("Session expired", "Your session expired or became invalid. Please retry.");
        return;
    }

    if (ovpnData.isEmpty()) {
        emit busyChanged(false);
        emit criticalOccurred("Download error", "The OVPN file was empty.");
        return;
    }

    QFile file(downloadedOvpnPathValue);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit busyChanged(false);
        emit criticalOccurred("Download error", QStringLiteral("Could not save the OVPN file to %1").arg(downloadedOvpnPathValue));
        return;
    }

    file.write(ovpnData);
    file.close();

    certificateDownloaded = true;
    saveCertificateState();

    emit busyChanged(false);
    emit statusMessage(QStringLiteral("Certificate downloaded to %1").arg(downloadedOvpnPathValue));
    emit savedCertificateAvailable(downloadedOvpnPathValue);
    emit informationOccurred("Download complete", QStringLiteral("OVPN file saved to:\n%1").arg(downloadedOvpnPathValue));
}