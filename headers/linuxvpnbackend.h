#ifndef LINUXVPNBACKEND_H
#define LINUXVPNBACKEND_H

#include "ivpnbackend.h"
#include <QProcess>
#include <QTimer>

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

    void disconnectVpn() override;
    VpnConnectionState connectionState() const override;

    static const ConnectionStepDefinition s_connectionSteps[];
    static const int s_connectionStepCount;

private slots:
    void onLogOutput();
    void fetchSessionStats();
    void parseStatsOutput();

private:
    QProcess logProcess;
    QProcess sessionStart;
    QTimer m_statsTimer;
    QProcess m_statsProcess;

    QString configPath;
    QString m_configName;
    VpnConnectionState connectedState = VpnConnectionState::Disconnected;
    int m_currentConnectionStep = -1;

    void setCurrentConnectionStep(int stepIndex);
    void parseConnectedLine(const QString &line);
};

#endif