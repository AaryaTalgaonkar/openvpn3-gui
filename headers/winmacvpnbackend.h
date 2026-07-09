#ifndef WINMACVPNBACKEND_H
#define WINMACVPNBACKEND_H

#include "ivpnbackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFutureWatcher>
#include <QProcess>
#include <QTcpSocket>
#include <QtConcurrent/QtConcurrent>

class IKeyStore;

class WinMacVpnBackend : public IVpnBackend
{
    Q_OBJECT

public:
    explicit WinMacVpnBackend(QObject *parent = nullptr);

    const ConnectionStepDefinition *connectionSteps() const override { return kWinMacConnectionSteps; }
    int connectionStepCount() const override { return kWinMacConnectionStepCount; }
    int currentConnectionStepIndex() const override { return m_currentConnectionStep; }

    void setKeyStore(IKeyStore *keystore);

    void connectVpn(const QString &ovpnPath,
                    const QString &password) override;

    void disconnectVpn() override;
    VpnConnectionState connectionState() const override;

    static const ConnectionStepDefinition kWinMacConnectionSteps[];
    static const int kWinMacConnectionStepCount;

private slots:
    void onMgmtReadyRead();
    void onSignDataFinished();

private:
    QProcess *vpnProcess = nullptr;
    QTcpSocket mgmtSocket;
    QString mgmtPassword;
    IKeyStore *m_keyStore = nullptr;
    VpnConnectionState connectedState = VpnConnectionState::Disconnected;
    int m_currentConnectionStep = -1;

    QFutureWatcher<QByteArray> m_signWatcher;

    QString resolveBinary() const;
    void handleMgmtLine(const QByteArray &line);
    void handleConnectedLog(const QString &payloadStr);

    int stateToStepIndex(const QString &stateStr) const;
    void setCurrentConnectionStep(int stepIndex);
};

#endif