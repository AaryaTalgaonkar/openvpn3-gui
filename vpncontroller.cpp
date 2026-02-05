#include "vpncontroller.h"
VpnController::VpnController(QObject *parent)
    : QObject(parent)
{
}

bool VpnController::isConnected() const
{
    return connectedState;
}

QString VpnController::resolveOpenVpnBinary() const
{
    QDir dir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    dir.cd("openvpn3");
    return dir.filePath("omicliagent.exe");
}

void VpnController::connectVpn(const QString &ovpnPath,
                               const QString &username,
                               const QString &password)
{
    if (vpnProcess)
        return;

    emit statusChanged("Connecting...");

    vpnProcess = new QProcess(this);

    QStringList args {
        "--config", ovpnPath,
        "--connection-timeout", "120",
        "--management", "127.0.0.1", "7505",
        "--management-query-passwords"
    };

    vpnProcess->start(resolveOpenVpnBinary(), args);

    mgmt = new OpenVpnMgmt("127.0.0.1", 7505, this);

    connect(mgmt, &OpenVpnMgmt::connected, this, [&]() {
        connectedState = true;
        emit connected();
        emit statusChanged("Disconnect");
    });

    connect(mgmt, &OpenVpnMgmt::authFailed, this, [&](const QString &msg) {
        emit errorOccurred(msg);
        vpnProcess = nullptr;
    });

    connect(mgmt, &OpenVpnMgmt::fatalError, this, [&](const QString &msg) {
        emit errorOccurred(msg);
        vpnProcess = nullptr;
    });

    mgmt->start(username, password);
}

void VpnController::disconnectVpn()
{
    if (!mgmt || !connectedState)
        return;

    emit statusChanged("Disconnecting...");
    mgmt->terminate();

    connectedState = false;
    vpnProcess = nullptr;
    emit disconnected();
    emit statusChanged("Connect");
}
