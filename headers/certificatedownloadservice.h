#pragma once

#include <QObject>
#include <QTimer>

class QNetworkReply;
class IKeyStore;

class CertificateDownloadService : public QObject
{
    Q_OBJECT

public:
    explicit CertificateDownloadService(QObject *parent = nullptr);
    ~CertificateDownloadService() override;

    void setKeyStore(IKeyStore *keystore);

    void loadSavedCertificateState();
    void startCertificateDownload(const QString &username, const QString &password);

    QString downloadedOvpnPath() const;

    void fetchGoogleTime();

signals:
    void savedCertificateAvailable(const QString &path);
    void savedCertificateUnavailable();
    void busyChanged(bool busy);
    void statusMessage(const QString &message);
    void informationOccurred(const QString &title, const QString &message);
    void warningOccurred(const QString &title, const QString &message);
    void criticalOccurred(const QString &title, const QString &message);

    void googleTimeReceived(const QDateTime &time);
    void googleTimeFetchFailed();

    void authenticationComplete(const QString &email);
    void csrGenerated(const QByteArray &csrData);
    void downloadProgressChanged(int percent);

private slots:
    void handleGoogleTimeReply(QNetworkReply *reply);
    void retryFetchGoogleTime();

private:
    void authenticateUser();
    void generateCsr();
    void submitCsr();

    IKeyStore *keystore = nullptr;
    QString currentUser;
    QString currentPassword;
    QString currentEmail;
    QByteArray currentCsrData;
    QTimer *googleRetryTimer = nullptr;
};