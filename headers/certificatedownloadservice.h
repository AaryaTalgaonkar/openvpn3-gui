#pragma once

#include <QObject>

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

private:
    QString currentUser;
};
