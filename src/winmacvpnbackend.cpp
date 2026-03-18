#include "winmacvpnbackend.h"

WinMacVpnBackend::WinMacVpnBackend(QObject *parent)
    : IVpnBackend(parent)
{
}

bool WinMacVpnBackend::isConnected() const
{
    return connectedState;
}

QString WinMacVpnBackend::resolveOpenVpnBinary() const
{
    QDir dir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    dir.cd("openvpn3");

    #ifdef Q_OS_WIN
        return dir.filePath("omicliagent.exe");
    #else
        return dir.filePath("omicliagent");
    #endif
}

void WinMacVpnBackend::connectVpn(const QString &ovpnPath,
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

    connect(vpnProcess, &QProcess::readyReadStandardOutput, this,
            [this, username, password]() {

                QByteArray output = vpnProcess->readAllStandardOutput();
                QString text = QString::fromUtf8(output);

                if (text.contains("OMI Listening") && !mgmt) {

                    mgmt = new OpenVpnMgmt("127.0.0.1", 7505, this);

                    connect(mgmt, &OpenVpnMgmt::connected, this, [&]() {
                        connectedState = true;
                        emit connected();
                        emit statusChanged("Disconnect");
                    });

                    connect(mgmt, &OpenVpnMgmt::authFailed, this,
                            [&](const QString &msg) {
                                emit errorOccurred(msg);
                                vpnProcess->deleteLater();
                                vpnProcess = nullptr;
                            });

                    connect(mgmt, &OpenVpnMgmt::fatalError, this,
                            [&](const QString &msg) {
                                emit errorOccurred(msg);
                                vpnProcess->deleteLater();
                                vpnProcess = nullptr;
                            });

                    mgmt->start(username, password);
                }
            });

}

void WinMacVpnBackend::disconnectVpn()
{
    if (!mgmt || !connectedState)
        return;

    emit statusChanged("Disconnecting...");

    mgmt->terminate();

    connectedState = false;

    if (vpnProcess) {
        vpnProcess->deleteLater();
        vpnProcess = nullptr;
    }

    emit disconnected();
    emit statusChanged("Connect");
}
