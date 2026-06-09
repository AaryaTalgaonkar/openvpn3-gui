#ifndef WINMACVPNBACKEND_H
#define WINMACVPNBACKEND_H

#include "ivpnbackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QTcpSocket>

class WinMacVpnBackend : public IVpnBackend
{
    Q_OBJECT

public:
    explicit WinMacVpnBackend(QObject *parent = nullptr);

    static const ConnectionStepDefinition *connectionSteps();
    static int connectionStepCount();

    void connectVpn(const QString &ovpnPath,
                    const QString &password) override;

    void updatePassword(const QString &password) override;

    void disconnectVpn() override;
    VpnConnectionState connectionState() const override;

private slots:
    void onMgmtReadyRead();

private:
    QProcess *vpnProcess = nullptr;
    QTcpSocket mgmtSocket;
    QString mgmtPassword;
    VpnConnectionState connectedState = VpnConnectionState::Disconnected;

    QString resolveOpenVpnBinary() const;
    void handleMgmtLine(const QByteArray &line);
    void handleConnectedLog(const QString &payloadStr);
};

#endif