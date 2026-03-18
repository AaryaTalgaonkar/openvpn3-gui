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

        if (line.startsWith(">PASSWORD:Need 'Auth' username/password")) {
            socket.write("state on\n");
            socket.write(QString("username Auth %1\n").arg(user).toUtf8());
            socket.write(QString("password Auth %1\n").arg(pass).toUtf8());
        }

        if (line.startsWith(">PASSWORD:Verification Failed")) {
            emit authFailed("Incorrect username/password");
        }

        if (line.startsWith(">FATAL:")) {
            QString msg;
            if (line.contains("parse_cert_crl_error")) {
                msg = "Error parsing CA certificate. The CA certificate may be corrupted or invalid.";
            }
            else if (line.contains("X509::parse_pem") && line.contains("bad base64 decode")) {
                msg = "Invalid certificate. Please check the certificate format.";
            }
            else if (line.contains("CERT_VERIFY_FAIL")) {
                msg = "Certificate verification failed. Probably your certificate has expired.";
            }
            else {
                msg = line;
            }

            emit fatalError(msg);
        }

        if (line.startsWith(">STATE:") && line.contains(",CONNECTED,")) {
            emit connected();
        }
    }
}
