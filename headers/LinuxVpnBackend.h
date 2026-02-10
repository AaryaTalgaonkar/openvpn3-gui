#ifndef LINUXVPNBACKEND_H
#define LINUXVPNBACKEND_H

#include <QObject>
#include <QProcess>

class LinuxVpnBackend : public QObject
{
    Q_OBJECT

public:
    explicit LinuxVpnBackend(QObject *parent = nullptr);

    void connectVpn(const QString &ovpnPath,
                    const QString &username,
                    const QString &password);

    void disconnectVpn();
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);
    void statusChanged(const QString &status);

private slots:
    void onLogOutput();
    void onSessionError();

private:
    QProcess logProcess;
    QProcess sessionStart;

    QString configPath;
    bool connectedState = false;
};

#endif
