#pragma once

#include <QObject>

class QByteArray;
class QNetworkAccessManager;
class QNetworkReply;
class QUrl;

class CertificateDownloadService : public QObject
{
    Q_OBJECT

public:
    explicit CertificateDownloadService(QObject *parent = nullptr);

    void loadSavedCertificateState();
    void startCertificateDownload(const QString &username, const QString &password);

    QString downloadedOvpnPath() const;

signals:
    void savedCertificateAvailable(const QString &path);
    void savedCertificateUnavailable();
    void busyChanged(bool busy);
    void statusMessage(const QString &message);
    void informationOccurred(const QString &title, const QString &message);
    void warningOccurred(const QString &title, const QString &message);
    void criticalOccurred(const QString &title, const QString &message);

private slots:
    void handleInitialResponse(QNetworkReply *reply);
    void handleDownloadResponse(QNetworkReply *reply);

private:
    QNetworkAccessManager *certManager = nullptr;
    QString currentUser;

    void sendCertificateRequest(const QUrl &url, const QByteArray &body);
    QString extractHiddenField(const QString &html, const QString &name) const;
    QString makeDownloadPath() const;
};