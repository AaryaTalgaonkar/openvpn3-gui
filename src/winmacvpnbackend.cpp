#include "winmacvpnbackend.h"

#include <QDateTime>
#include <QStringList>

#include <unordered_map>

const ConnectionStepDefinition kWinMacConnectionSteps[] = {
    {"RESOLVE", "🔎", "Resolving server address"},
    {"WAIT", "⌛", "Waiting for server response"},
    {"CONNECTING", "↻", "Connecting to server"},
    {"GET_CONFIG", "⚙", "Downloading configuration options"},
    {"ASSIGN_IP", "🧭", "Assigning IP address"},
};

constexpr int kWinMacConnectionStepCount = static_cast<int>(sizeof(kWinMacConnectionSteps) / sizeof(kWinMacConnectionSteps[0]));

namespace {
constexpr auto kMgmtHost = "127.0.0.1";
constexpr quint16 kMgmtPort = 7505;

const QStringList kIgnoredLogPrefixes = {
    QStringLiteral("SUCCESS:"),
    QStringLiteral("ERROR:")
};

const std::unordered_map<std::string, std::string> kFatalEventText = {
    {"auth-failure:", "The password that you entered is incorrect. Repeatedly entering an incorrect password may result in a temporary block. Please try again."},
    {"CERT_VERIFY_FAIL", "The certificate seems to have expired. Please contact CSC support to renew your certificate."},
    {"TLS_VERSION_MIN", "The server does not support the required TLS version."},
    {"TLS_ALERT_PROTOCOL_VERSION", "A secure protocol version could not be negotiated with the server."},
    {"TLS_ALERT_UNKNOWN_CA", "The certificate authority is not trusted by this client."},
    {"TLS_ALERT_MISC", "A TLS certificate or handshake error occurred."},
    {"TLS_ALERT_HANDSHAKE_FAILURE", "Secure TLS handshake failed."},
    {"TLS_ALERT_CERTIFICATE_EXPIRED", "A certificate used in the connection has expired."},
    {"TLS_ALERT_CERTIFICATE_REVOKED", "A certificate used in the connection has been revoked."},
    {"TLS_ALERT_BAD_CERTIFICATE", "A certificate used in the connection is invalid."},
    {"TLS_ALERT_UNSUPPORTED_CERTIFICATE", "The certificate type or key usage is not supported."},
    {"TLS_SIGALG_DISALLOWED_OR_UNSUPPORTED", "The certificate signature algorithm is not allowed or not supported."},
    {"CLIENT_HALT", "The server requested that the client stop."},
    {"CLIENT_SETUP", "Client setup failed. Please review your VPN profile and local settings."},
    {"TUN_HALT", "The virtual network adapter was stopped."},
    {"CONNECTION_TIMEOUT", "Connection timed out while contacting the VPN server. Please check your network connection and try again."},
    {"INACTIVE_TIMEOUT", "Connection closed due to inactivity timeout."},
    {"DYNAMIC_CHALLENGE", "Additional verification is required to continue sign-in."},
    {"PROXY_NEED_CREDS", "Proxy authentication is required."},
    {"PROXY_ERROR", "A proxy error prevented the VPN connection."},
    {"TUN_SETUP_FAILED", "Failed to configure the virtual network adapter."},
    {"TUN_IFACE_CREATE", "Failed to create the virtual network adapter."},
    {"TUN_IFACE_DISABLED", "The virtual network adapter is disabled."},
    {"EPKI_ERROR", "Could not access the external certificate/key provider."},
    {"EPKI_INVALID_ALIAS", "The selected external certificate/key alias is invalid."},
    {"RELAY_ERROR", "Relay setup failed."},
    {"COMPRESS_ERROR", "Compression/decompression failed."},
    {"NTLM_MISSING_CRYPTO", "Required cryptographic support for NTLM is not available."},
    {"SESSION_EXPIRED", "Your VPN session has expired. Please sign in again."},
    {"NEED_CREDS", "Credentials are required to continue."},
};

QString friendlyFatalMessageFromLine(const QByteArray &line)
{
    const QString lineStr = QString::fromUtf8(line).trimmed();

    const int firstColon = lineStr.indexOf(':');
    const QString afterPrefix = firstColon >= 0 ? lineStr.mid(firstColon + 1).trimmed() : lineStr;
    const int commaPos = afterPrefix.indexOf(',');
    const QString eventName = (commaPos >= 0 ? afterPrefix.left(commaPos) : afterPrefix).trimmed();

    const auto it = kFatalEventText.find(eventName.toStdString());
    if (it != kFatalEventText.end()) {
        return QString::fromStdString(it->second);
    }

    return lineStr;
}

QString formatMgmtLogLine(const QByteArray &payload)
{
    const QString payloadStr = QString::fromUtf8(payload).trimmed();
    const int firstComma = payloadStr.indexOf(',');
    if (firstComma < 0) {
        return payloadStr;
    }

    const qint64 epochSeconds = payloadStr.left(firstComma).toLongLong();
    const QDateTime timestamp = QDateTime::fromSecsSinceEpoch(epochSeconds).toLocalTime();
    const int secondComma = payloadStr.indexOf(',', firstComma + 1);
    const QString message = secondComma >= 0
        ? payloadStr.mid(secondComma + 1).trimmed()
        : payloadStr.mid(firstComma + 1).trimmed();

    if (!timestamp.isValid()) {
        return message;
    }

    return QStringLiteral("[%1] %2").arg(timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), message);
}
}

const ConnectionStepDefinition *WinMacVpnBackend::connectionSteps()
{
    return kWinMacConnectionSteps;
}

int WinMacVpnBackend::connectionStepCount()
{
    return kWinMacConnectionStepCount;
}

WinMacVpnBackend::WinMacVpnBackend(QObject *parent)
    : IVpnBackend(parent)
{
    connect(&mgmtSocket, &QTcpSocket::readyRead,
            this, &WinMacVpnBackend::onMgmtReadyRead);
}

VpnConnectionState WinMacVpnBackend::connectionState() const
{
    return connectedState;
}

QString WinMacVpnBackend::resolveOpenVpnBinary() const
{
    QDir dir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    dir.cd("backend");

    #ifdef Q_OS_WIN
        return dir.filePath("iitdvpncli.exe");
    #else
        return dir.filePath("iitdvpncli");
    #endif
}

void WinMacVpnBackend::connectVpn(const QString &ovpnPath,
                                   const QString &password)
{
    if (vpnProcess)
        return;

    mgmtPassword = password;
    connectedState = VpnConnectionState::Connecting;
    emit connectionStateChanged(connectedState);
    vpnProcess = new QProcess(this);

    QStringList args {
        "--config", ovpnPath,
        "--connection-timeout", "30",
        "--management", "127.0.0.1", "7505",
        "--management-query-passwords",
        "--auth-use-cert-cn-username"
    };

    vpnProcess->start(resolveOpenVpnBinary(), args);

    connect(vpnProcess, &QProcess::readyReadStandardOutput, this,
            [this]() {

                QByteArray output = vpnProcess->readAllStandardOutput();
                QString text = QString::fromUtf8(output);

                if (text.contains("OMI Listening") && mgmtSocket.state() == QAbstractSocket::UnconnectedState) {
                    mgmtSocket.connectToHost(QString::fromUtf8(kMgmtHost), kMgmtPort);
                }
            });

    connect(vpnProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this](int , QProcess::ExitStatus ) {
                if (vpnProcess) {
                    vpnProcess->deleteLater();
                    vpnProcess = nullptr;
                }
            });

}

void WinMacVpnBackend::updatePassword(const QString &password)
{
    mgmtPassword = password;
}

void WinMacVpnBackend::disconnectVpn()
{
    if (mgmtSocket.state() != QAbstractSocket::ConnectedState || connectedState != VpnConnectionState::Connected)
        return;

    mgmtSocket.write("signal SIGTERM\n");
    mgmtSocket.flush();
}

void WinMacVpnBackend::onMgmtReadyRead()
{
    while (mgmtSocket.canReadLine()) {
        handleMgmtLine(mgmtSocket.readLine().trimmed());
    }
}

void WinMacVpnBackend::handleMgmtLine(const QByteArray &line)
{
    qDebug().noquote() << "[MGMT]" << line;

    if (!line.startsWith('>')) {
        const QString continuation = QString::fromUtf8(line).trimmed();
        if (!continuation.isEmpty()) {
            if (continuation.startsWith(QStringLiteral("eval config error"), Qt::CaseInsensitive)) {
                emit errorOccurred(QStringLiteral("The .ovpn file is malformed. Please download the file again."));
                return;
            }
            if (continuation.startsWith(QStringLiteral("creds error"), Qt::CaseInsensitive)) {
                emit errorOccurred(QStringLiteral("The dynamic challenge cookie from the server was malformed. Please try again."));
                return;
            }
            for (const QString &prefix : kIgnoredLogPrefixes) {
                if (continuation.startsWith(prefix, Qt::CaseInsensitive)) {
                    return;
                }
            }
            emit logLineReceived(continuation);
        }
        return;
    }

    const int tagEnd = line.indexOf(':', 1);
    if (tagEnd <= 1) {
        return;
    }

    const QString tag = QString::fromUtf8(line.mid(1, tagEnd - 1));
    const QByteArray payload = line.mid(tagEnd + 1);

    if (tag == QStringLiteral("PASSWORD")) {
        if (payload.startsWith("Need 'Auth' password")) {
            mgmtSocket.write("log on\n");
            mgmtSocket.write("state on\n");
            mgmtSocket.write("bytecount 1\n");
            mgmtSocket.write(QString("password Auth %1\n").arg(mgmtPassword).toUtf8());
        }
    } else if (tag == QStringLiteral("FATAL")) {
        emit errorOccurred(friendlyFatalMessageFromLine(line));
    } else if (tag == QStringLiteral("STATE")) {
        const QString statePayload = QString::fromUtf8(payload);
        const QStringList parts = statePayload.split(',');

        if (parts.size() >= 2) {
            const QString stateStr = parts.at(1).trimmed();
            if (!stateStr.isEmpty()) {
                emit stateChanged(stateStr);
                if (stateStr == QStringLiteral("CONNECTED")) {
                    connectedState = VpnConnectionState::Connected;
                    emit connected();
                    emit connectionStateChanged(connectedState);
                } else if (stateStr == QStringLiteral("EXITING")) {
                    connectedState = VpnConnectionState::Disconnected;
                    emit disconnected();
                    emit connectionStateChanged(connectedState);
                } else if (stateStr == QStringLiteral("RECONNECTING")) {
                    connectedState = VpnConnectionState::Connecting;
                    emit connectionStateChanged(connectedState);
                }
            }
        }
    } else if (tag == QStringLiteral("LOG")) {  

        const QString payloadStr = QString::fromUtf8(payload).trimmed();
        emit logLineReceived(formatMgmtLogLine(payload));
        handleConnectedLog(payloadStr);

    } else if (tag == QStringLiteral("BYTECOUNT")) {
        const QStringList parts = QString::fromUtf8(payload).split(',');
        if (parts.size() < 2) {
            return;
        }

        bool uploadOk = false;
        bool downloadOk = false;
        const qulonglong uploadBytes = parts.at(1).trimmed().toULongLong(&uploadOk);
        const qulonglong downloadBytes = parts.at(0).trimmed().toULongLong(&downloadOk);
        if (uploadOk && downloadOk) {
            emit byteCountChanged(uploadBytes, downloadBytes);
        }
    }
}

void WinMacVpnBackend::handleConnectedLog(const QString &payloadStr)
{
    if (!payloadStr.contains("CONNECTED")) {
        return;
    }

    const int connIdx = payloadStr.indexOf("CONNECTED");
    QString rest = payloadStr.mid(connIdx + QStringLiteral("CONNECTED").length());
    const int colon = rest.indexOf(':');
    if (colon >= 0) rest = rest.mid(colon + 1);
    rest = rest.trimmed();

    const int viaIdx = rest.indexOf(" via ");
    QString left = viaIdx >= 0 ? rest.left(viaIdx).trimmed() : rest.trimmed();
    QString right = viaIdx >= 0 ? rest.mid(viaIdx + 5).trimmed() : QString();

    QString remote = left;
    QString remoteAddr;
    const int paren = left.indexOf('(');
    if (paren >= 0) {
        remote = left.left(paren).trimmed();
        const int closeParen = left.indexOf(')', paren);
        if (closeParen > paren) {
            remoteAddr = left.mid(paren + 1, closeParen - paren - 1).trimmed();
        }
    }

    QString proto;
    QString localIface;
    QString localIp;
    QString gateway;
    int mtu = 0;

    if (!right.isEmpty()) {
        if (right.startsWith('/')) {
            const int space = right.indexOf(' ');
            if (space > 1) proto = right.mid(1, space - 1);
        }

        const int onIdx = right.indexOf(" on ");
        QString afterOn = onIdx >= 0 ? right.mid(onIdx + 4).trimmed() : right;

        const QStringList parts = afterOn.split(' ', Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            const QString ifaceIp = parts.at(0);
            const QStringList seg = ifaceIp.split('/', Qt::SkipEmptyParts);
            if (seg.size() >= 1) localIface = seg.at(0);
            if (seg.size() >= 2) localIp = seg.at(1);
        }

        const int gwIdx = right.indexOf("gw=[");
        if (gwIdx >= 0) {
            const int gwStart = gwIdx + 4;
            const int gwEnd = right.indexOf(']', gwStart);
            if (gwEnd > gwStart) gateway = right.mid(gwStart, gwEnd - gwStart).trimmed();
        }

        const int mtuIdx = right.indexOf("mtu=");
        if (mtuIdx >= 0) {
            const int mtuStart = mtuIdx + 4;
            QString mtuStr;
            for (int i = mtuStart; i < right.size(); ++i) {
                const QChar c = right.at(i);
                if (c.isDigit()) mtuStr.append(c); else break;
            }
            mtu = mtuStr.toInt();
        }
    }

    emit connectionInfoChanged(remote, remoteAddr, proto, localIface, localIp, gateway, mtu);
}
