#include "openvpnmgmt.h"

OpenVpnMgmt::OpenVpnMgmt(const QString &host, quint16 port, QObject *parent)
    : QObject(parent)
{
    socket.connectToHost(host, port);
    connect(&socket, &QTcpSocket::readyRead, this, &OpenVpnMgmt::onReadyRead);
}

void OpenVpnMgmt::start(const QString &username, const QString &password)
{
    user = username;
    pass = password;
}

void OpenVpnMgmt::terminate()
{
    socket.write("signal SIGTERM\n");
    socket.flush();
}

void OpenVpnMgmt::onReadyRead()
{
    while (socket.canReadLine()) {

        QByteArray line = socket.readLine().trimmed();
        qDebug().noquote() << "[MGMT]" << line;

        if (ignoreCount > 0) {
            ignoreCount--;
            continue;
        }

        switch (phase) {
        case 0:
            socket.write("state on\n");
            phase++;
            return;
        case 1:
            socket.write(QString("username Auth %1\n").arg(user).toUtf8());
            phase++;
            return;
        case 2:
            socket.write(QString("password Auth %1\n").arg(pass).toUtf8());
            phase++;
            return;
        }

        if (line.startsWith(">PASSWORD:Verification Failed")) {
            emit authFailed("Incorrect username/password");
        }

        if (line.startsWith(">FATAL:")) {
            emit fatalError(line);
        }

        if (line.startsWith(">STATE:") && line.contains(",CONNECTED,")) {
            emit connected();
        }
    }
}
