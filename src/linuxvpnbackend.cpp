#include "linuxvpnbackend.h"
#include <QDebug>

const ConnectionStepDefinition LinuxVpnBackend::s_connectionSteps[] = {
    {"SESSION_START", "🔗", "Starting VPN session"},
    {"AUTH", "🔑", "Authenticating"},
    {"CONNECTING", "↻", "Connecting"},
    {"CONNECTED", "✓", "Connected"},
};

const int LinuxVpnBackend::s_connectionStepCount = sizeof(s_connectionSteps) / sizeof(s_connectionSteps[0]);

LinuxVpnBackend::LinuxVpnBackend(QObject *parent)
    : IVpnBackend(parent)
{
    connect(&logProcess, &QProcess::readyReadStandardOutput,
            this, &LinuxVpnBackend::onLogOutput);

}

VpnConnectionState LinuxVpnBackend::connectionState() const
{
    return connectedState;
}

void LinuxVpnBackend::setCurrentConnectionStep(int stepIndex)
{
    if (m_currentConnectionStep != stepIndex) {
        m_currentConnectionStep = stepIndex;
        emit connectionStepChanged(stepIndex);
    }
}

void LinuxVpnBackend::updatePassword(const QString &password)
{
    Q_UNUSED(password);
}

void LinuxVpnBackend::connectVpn(const QString &ovpnPath,
                                 const QString &password)
{
    if (logProcess.state() != QProcess::NotRunning)
        return;

    configPath = ovpnPath;
    connectedState = VpnConnectionState::Connecting;
    emit connectionStateChanged(connectedState);

    logProcess.start("iitdvpn", {
                                     "log",
                                     "--config", ovpnPath
                                 });

    sessionStart.start("iitdvpn", {
                                       "session-start",
                                       "--config", ovpnPath
                                   });

    sessionStart.write(QString("%1\n").arg(password).toUtf8());
}

void LinuxVpnBackend::disconnectVpn()
{
    if (connectedState != VpnConnectionState::Connected)
        return;

    QProcess::execute("iitdvpn", {
                                      "session-manage",
                                      "--config", configPath,
                                      "--disconnect"
                                  });

    connectedState = VpnConnectionState::Disconnected;
    m_currentConnectionStep = -1;
    emit connectionStepChanged(-1);
    emit disconnected();
    emit connectionStateChanged(connectedState);

    logProcess.terminate();
}


void LinuxVpnBackend::onLogOutput()
{
    while (logProcess.canReadLine()) {

        QString line = QString::fromUtf8(logProcess.readLine()).trimmed();
        qDebug().noquote() << "[OVPN3]" << line;
        if (!line.contains("[STATUS]"))
            continue;

        if (line.contains("Session state: CONNECTING") || line.contains("Connecting to")) {
            setCurrentConnectionStep(0);
        }
        else if (line.contains("Authenticating") || line.contains("Authentication")) {
            setCurrentConnectionStep(1);
        }
        else if (line.contains("client connecting") || line.contains("Establishing")) {
            setCurrentConnectionStep(2);
        }
        else if (line.contains("Client connected")) {
            setCurrentConnectionStep(3);
            connectedState = VpnConnectionState::Connected;
            emit connected();
            emit connectionStateChanged(connectedState);
        }
        else if (line.contains("Authentication failed")) {
            setCurrentConnectionStep(-1);
            emit errorOccurred("Incorrect username/password");
        }
        else if (line.contains("parse_cert_crl_error")) {
            setCurrentConnectionStep(-1);
            emit errorOccurred("Error parsing CA certificate. The CA certificate may be corrupted or invalid.");
        }
        else if(line.contains("parse_pem")){
            setCurrentConnectionStep(-1);
            emit errorOccurred("Invalid certificate. Please check the certificate format.");
        }
        else if(line.contains("Certificate verification failed")){
            setCurrentConnectionStep(-1);
            emit errorOccurred("Certificate verification failed. Probably your certificate has expired.");
        }
    }
}