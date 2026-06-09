#ifndef LINUXVPNBACKEND_H
#define LINUXVPNBACKEND_H

#include "ivpnbackend.h"
#include <QProcess>

class LinuxVpnBackend : public IVpnBackend
{
    Q_OBJECT
public:
    explicit LinuxVpnBackend(QObject *parent = nullptr);

    const ConnectionStepDefinition *connectionSteps() const override { return s_connectionSteps; }
    int connectionStepCount() const override { return s_connectionStepCount; }
    int currentConnectionStepIndex() const override { return m_currentConnectionStep; }

    void connectVpn(const QString &ovpnPath,
                    const QString &password) override;

    void updatePassword(const QString &password) override;

    void disconnectVpn() override;
    VpnConnectionState connectionState() const override;

    /// Static definitions — consumers can reference these directly
    static const ConnectionStepDefinition s_connectionSteps[];
    static const int s_connectionStepCount;

private slots:
    void onLogOutput();

private:
    QProcess logProcess;
    QProcess sessionStart;

    QString configPath;
    VpnConnectionState connectedState = VpnConnectionState::Disconnected;
    int m_currentConnectionStep = -1;

    void setCurrentConnectionStep(int stepIndex);
};

#endif