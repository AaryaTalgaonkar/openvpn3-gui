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

    const ConnectionStepDefinition *connectionSteps() const override { return kWinMacConnectionSteps; }
    int connectionStepCount() const override { return kWinMacConnectionStepCount; }
    int currentConnectionStepIndex() const override { return m_currentConnectionStep; }

    void connectVpn(const QString &ovpnPath,
                    const QString &password) override;

    void updatePassword(const QString &password) override;

    void disconnectVpn() override;
    VpnConnectionState connectionState() const override;

    /// Static definitions — consumers can reference these directly
    static const ConnectionStepDefinition kWinMacConnectionSteps[];
    static const int kWinMacConnectionStepCount;

private slots:
    void onMgmtReadyRead();

private:
    QProcess *vpnProcess = nullptr;
    QTcpSocket mgmtSocket;
    QString mgmtPassword;
    VpnConnectionState connectedState = VpnConnectionState::Disconnected;
    int m_currentConnectionStep = -1;

    QString resolveOpenVpnBinary() const;
    void handleMgmtLine(const QByteArray &line);
    void handleConnectedLog(const QString &payloadStr);

    /// Map an OpenVPN management state string (e.g. "RESOLVE", "WAIT") to a step index,
    /// or -1 if it doesn't map to any step.
    int stateToStepIndex(const QString &stateStr) const;
    void setCurrentConnectionStep(int stepIndex);
};

#endif