#include "linuxvpnbackend.h"
#include <QDebug>

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

void LinuxVpnBackend::connectVpn(const QString &ovpnPath,
                                 const QString &password)
{
    if (logProcess.state() != QProcess::NotRunning)
        return;

    configPath = ovpnPath;
    connectedState = VpnConnectionState::Connecting;
    emit connectionStateChanged(connectedState);

    // 1️⃣ Start log watcher FIRST
    logProcess.start("openvpn3", {
                                     "log",
                                     "--config", ovpnPath,
                                     "--log-level", "0"
                                 });

    // 2️⃣ Start session
    sessionStart.start("openvpn3", {
                                       "session-start",
                                       "--config", ovpnPath
                                   });

    // 3️⃣ Provide credentials
    sessionStart.write(QString("%1\n").arg(password).toUtf8());
}

void LinuxVpnBackend::disconnectVpn()
{
    if (connectedState != VpnConnectionState::Connected)
        return;

    QProcess::execute("openvpn3", {
                                      "session-manage",
                                      "--config", configPath,
                                      "--disconnect"
                                  });

    connectedState = VpnConnectionState::Disconnected;
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

        if (line.contains("Client connected")) {
            connectedState = VpnConnectionState::Connected;
            emit connected();
            emit connectionStateChanged(connectedState);
        }
        else if (line.contains("Authentication failed")) {
            emit errorOccurred("Incorrect username/password");
        }
        else if (line.contains("parse_cert_crl_error")) {
            emit errorOccurred("Error parsing CA certificate. The CA certificate may be corrupted or invalid.");
        }
        else if(line.contains("parse_pem")){
            emit errorOccurred("Invalid certificate. Please check the certificate format.");
        }
        else if(line.contains("Certificate verification failed")){
            emit errorOccurred("Certificate verification failed. Probably your certificate has expired.");
        }
    }
}
