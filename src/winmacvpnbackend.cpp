#include "winmacvpnbackend.h"
#include "ikeystore.h"

#include <QDateTime>
#include <QStringList>


const ConnectionStepDefinition WinMacVpnBackend::kWinMacConnectionSteps[] = {
    {"RESOLVE", "🔎", "Resolving server address"},
    {"WAIT", "⌛", "Waiting for server response"},
    {"CONNECTING", "↻", "Connecting to server"},
    {"GET_CONFIG", "⚙", "Downloading configuration options"},
    {"ASSIGN_IP", "🧭", "Assigning IP address"},
};

const int WinMacVpnBackend::kWinMacConnectionStepCount =
    sizeof(WinMacVpnBackend::kWinMacConnectionSteps) / sizeof(WinMacVpnBackend::kWinMacConnectionSteps[0]);


namespace {
constexpr auto kMgmtHost = "127.0.0.1";
constexpr quint16 kMgmtPort = 7505;

const QStringList kIgnoredLogPrefixes = {
    QStringLiteral("SUCCESS:"),
    QStringLiteral("ERROR:")
};

QString friendlyFatalMessageFromLine(const QByteArray &line, const QString &currentOperation)
{
    const QString lineStr = QString::fromUtf8(line).trimmed();

    const int firstColon = lineStr.indexOf(':');
    if (firstColon < 0) {
        return lineStr;
    }

    const QString afterPrefix = lineStr.mid(firstColon + 1).trimmed();

    const int eventColon = afterPrefix.indexOf(':');
    const QString eventType = (eventColon >= 0)
        ? afterPrefix.left(eventColon).trimmed()
        : afterPrefix;

    if (eventType == QStringLiteral("auth-failure")) {
        return QStringLiteral("The password that you entered is incorrect. Repeatedly entering an incorrect password may result in a temporary block. Please try again.");
    }

    if (eventType == QStringLiteral("CONNECTION_TIMEOUT")) {
        if (currentOperation == QStringLiteral("RESOLVE")) {
            return QStringLiteral("The DNS resolution could not be done. Please check your internet connection and try once again.");
        }

        if (currentOperation == QStringLiteral("WAIT")) {
            return QStringLiteral("The server could not be reached or it failed to respond. Common causes are :\n"
                                 "i. You are already on IITD network. Please connect externally.\n"
                                 "ii. You are behind a firewall blocking port 1194. Please try to connect with your mobile hotspot instead.");
        }

        return QStringLiteral("The connection timed out while contacting the server. Please try again.");
    }

    return afterPrefix;
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
        "--connection-timeout", "10",
        "--management", "127.0.0.1", "7505",
        "--management-query-passwords",
        "--auth-use-cert-cn-username",
        "--management-external-key"
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

int WinMacVpnBackend::stateToStepIndex(const QString &stateStr) const
{
    for (int i = 0; i < kWinMacConnectionStepCount; ++i) {
        if (QString::fromUtf8(kWinMacConnectionSteps[i].state) == stateStr) {
            return i;
        }
    }
    return -1;
}

void WinMacVpnBackend::setCurrentConnectionStep(int stepIndex)
{
    if (m_currentConnectionStep != stepIndex) {
        m_currentConnectionStep = stepIndex;
        emit connectionStepChanged(stepIndex);
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
        QString operation;
        if (m_currentConnectionStep >= 0 && m_currentConnectionStep < kWinMacConnectionStepCount) {
            operation = QString::fromUtf8(kWinMacConnectionSteps[m_currentConnectionStep].state);
        }
        emit errorOccurred(friendlyFatalMessageFromLine(line, operation));
    } else if (tag == QStringLiteral("STATE")) {
        const QString statePayload = QString::fromUtf8(payload);
        const QStringList parts = statePayload.split(',');

        if (parts.size() >= 2) {
            const QString stateStr = parts.at(1).trimmed();
            if (!stateStr.isEmpty()) {
                emit stateChanged(stateStr);
                if (stateStr == QStringLiteral("CONNECTED")) {
                    setCurrentConnectionStep(-1);
                    connectedState = VpnConnectionState::Connected;
                    emit connected();
                    emit connectionStateChanged(connectedState);
                } else if (stateStr == QStringLiteral("EXITING")) {
                    setCurrentConnectionStep(-1);
                    connectedState = VpnConnectionState::Disconnected;
                    emit disconnected();
                    emit connectionStateChanged(connectedState);
                } else if (stateStr == QStringLiteral("RECONNECTING")) {
                    setCurrentConnectionStep(-1);
                    connectedState = VpnConnectionState::Connecting;
                    emit connectionStateChanged(connectedState);
                } else {
                    // Map intermediate state strings (RESOLVE, WAIT, etc.) to step index
                    const int stepIdx = stateToStepIndex(stateStr);
                    if (stepIdx >= 0) {
                        setCurrentConnectionStep(stepIdx);
                    }
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
    } else if(tag == QStringLiteral("RSA_SIGN")) {
        if (!m_keyStore) {
            qWarning() << "[WinMacVpnBackend] RSA_SIGN received but no keystore is set.";
            return;
        }

        const QByteArray rawData = QByteArray::fromBase64(payload);
        if (rawData.isEmpty()) {
            qWarning() << "[WinMacVpnBackend] RSA_SIGN: failed to decode base64 payload.";
            return;
        }

        const QByteArray signature = m_keyStore->signData(rawData);
        if (signature.isEmpty()) {
            qWarning() << "[WinMacVpnBackend] RSA_SIGN: keystore returned empty signature.";
            return;
        }


        mgmtSocket.write("rsa-sig\n");
        mgmtSocket.write(signature.toBase64() + "\n");
        mgmtSocket.write("END\n");
        mgmtSocket.flush();
    }
}

void WinMacVpnBackend::setKeyStore(IKeyStore *keystore)
{
    m_keyStore = keystore;
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
