#include "linuxvpnbackend.h"
#include <QDebug>
#include <QUuid>

const ConnectionStepDefinition LinuxVpnBackend::s_connectionSteps[] = {
    {"CONNECTING", "↻", "Connecting"},
    {"CONNECTED",  "✓", "Connected"},
};

const int LinuxVpnBackend::s_connectionStepCount = sizeof(s_connectionSteps) / sizeof(s_connectionSteps[0]);

LinuxVpnBackend::LinuxVpnBackend(QObject *parent)
    : IVpnBackend(parent)
{
    connect(&logProcess, &QProcess::readyReadStandardOutput,
            this, &LinuxVpnBackend::onLogOutput);

    connect(&m_statsTimer, &QTimer::timeout,
            this, &LinuxVpnBackend::fetchSessionStats);

    connect(&m_statsProcess, &QProcess::readyReadStandardOutput,
            this, &LinuxVpnBackend::parseStatsOutput);

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

void LinuxVpnBackend::connectVpn(const QString &ovpnPath,
                                 const QString &password)
{
    if (logProcess.state() != QProcess::NotRunning)
        return;

    configPath = ovpnPath;

    m_configName = QUuid::createUuid().toString(QUuid::WithoutBraces);

    int importResult = QProcess::execute("iitdvpn", {
        "config-import",
        "--config", ovpnPath,
        "--name", m_configName
    });

    if (importResult != 0) {
        m_configName.clear();
        setCurrentConnectionStep(-1);
        emit errorOccurred("The configuration file seems to be corrupted. Please download the configuration file again.");
        return;
    }

    connectedState = VpnConnectionState::Connecting;
    emit connectionStateChanged(connectedState);

    logProcess.start("iitdvpn", {
                                     "log",
                                     "--config", m_configName
                                 });

    sessionStart.start("iitdvpn", {
                                       "session-start",
                                       "--config", m_configName,
                                       "--connection-timeout", "10"
                                   });

    sessionStart.write(QString("%1\n").arg(password).toUtf8());
}

void LinuxVpnBackend::disconnectVpn()
{
    if (connectedState != VpnConnectionState::Connected)
        return;

    QProcess::execute("iitdvpn", {
                                      "session-manage",
                                      "--config", m_configName,
                                      "--disconnect"
                                  });

    connectedState = VpnConnectionState::Disconnected;
    m_currentConnectionStep = -1;
    emit connectionStepChanged(-1);
    emit disconnected();
    emit connectionStateChanged(connectedState);

    m_statsTimer.stop();
    m_statsProcess.kill();
    logProcess.terminate();

    if (!m_configName.isEmpty()) {
        QProcess configRemove;
        configRemove.start("iitdvpn", {"config-remove", "--config", m_configName});
        if (configRemove.waitForStarted()) {
            configRemove.write("YES\n");
            configRemove.closeWriteChannel();
            configRemove.waitForFinished();
        }
        m_configName.clear();
    }
}


void LinuxVpnBackend::parseConnectedLine(const QString &line)
{
    int connPos = line.indexOf("Connected: ");
    if (connPos < 0)
        return;

    QString rest = line.mid(connPos + 11).trimmed();

    int viaIdx = rest.indexOf(" via ");
    QString left = viaIdx >= 0 ? rest.left(viaIdx).trimmed() : rest.trimmed();
    QString right = viaIdx >= 0 ? rest.mid(viaIdx + 5).trimmed() : QString();

    QString remote = left;
    QString remoteAddr;
    int paren = left.indexOf('(');
    if (paren >= 0) {
        remote = left.left(paren).trimmed();
        int closeParen = left.indexOf(')', paren);
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
            int space = right.indexOf(' ');
            if (space > 1) proto = right.mid(1, space - 1);
        }

        int onIdx = right.indexOf(" on ");
        QString afterOn = onIdx >= 0 ? right.mid(onIdx + 4).trimmed() : right;

        const QStringList parts = afterOn.split(' ', Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            const QString ifaceIp = parts.at(0);
            const QStringList seg = ifaceIp.split('/', Qt::SkipEmptyParts);
            if (seg.size() >= 1) localIface = seg.at(0);
            if (seg.size() >= 2) localIp = seg.at(1);
        }

        int gwIdx = right.indexOf("gw=[");
        if (gwIdx >= 0) {
            int gwStart = gwIdx + 4;
            int gwEnd = right.indexOf(']', gwStart);
            if (gwEnd > gwStart) gateway = right.mid(gwStart, gwEnd - gwStart).trimmed();
        }

        int mtuIdx = right.indexOf("mtu=");
        if (mtuIdx >= 0) {
            int mtuStart = mtuIdx + 4;
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

void LinuxVpnBackend::fetchSessionStats()
{
    if (m_statsProcess.state() != QProcess::NotRunning)
        return;

    m_statsProcess.start("iitdvpn", {"session-stats", "--config", m_configName});
}

void LinuxVpnBackend::parseStatsOutput()
{
    QByteArray data = m_statsProcess.readAllStandardOutput();
    qulonglong bytesIn = 0, bytesOut = 0;

    for (const QByteArray &line : data.split('\n')) {
        if (line.contains("BYTES_IN")) {
            int dot = line.lastIndexOf('.');
            if (dot >= 0)
                bytesIn = line.mid(dot + 1).trimmed().toULongLong();
        } else if (line.contains("BYTES_OUT")) {
            int dot = line.lastIndexOf('.');
            if (dot >= 0)
                bytesOut = line.mid(dot + 1).trimmed().toULongLong();
        }
    }

    if (bytesIn > 0 || bytesOut > 0) {
        emit byteCountChanged(bytesOut, bytesIn);
    }
}

void LinuxVpnBackend::onLogOutput()
{
    while (logProcess.canReadLine()) {

        QString line = QString::fromUtf8(logProcess.readLine()).trimmed();
        qDebug().noquote() << "[OVPN3]" << line;
        emit logLineReceived(line);

        if (line.contains("Client INFO: Connected:")) {
            parseConnectedLine(line);
        }

        if (!line.contains("[STATUS] Connection"))
            continue;

        int statusPos = line.indexOf("[STATUS] Connection");
        if (statusPos < 0)
            continue;

        int commaPos = line.indexOf(", ", statusPos);
        if (commaPos < 0)
            continue;

        QString afterStatus = line.mid(commaPos + 2).trimmed();

        QString statusMinor;
        int colonPos = afterStatus.indexOf(": ");
        if (colonPos >= 0)
            statusMinor = afterStatus.left(colonPos).trimmed();
        else
            statusMinor = afterStatus.trimmed();

        if (statusMinor == "Client connecting") {
            setCurrentConnectionStep(0);
            emit stateChanged(QString::fromUtf8(s_connectionSteps[0].state));
        }
        else if (statusMinor == "Client reconnect" || statusMinor == "Client connection resuming") {
            setCurrentConnectionStep(0);
            emit stateChanged(QString::fromUtf8(s_connectionSteps[0].state));
        }
        else if (statusMinor == "Client connected") {
            setCurrentConnectionStep(1); 
            emit stateChanged(QString::fromUtf8(s_connectionSteps[1].state));
            connectedState = VpnConnectionState::Connected;
            emit connected();
            emit connectionStateChanged(connectedState);
            m_statsTimer.start(1000);
        }
        else if (statusMinor == "Client authentication failed") {
            setCurrentConnectionStep(-1);
            emit errorOccurred("The password that you entered is incorrect. Repeatedly entering an incorrect password may result in a temporary block. Please try again.");
        }
        else if (statusMinor == "Client connection failed") {
            setCurrentConnectionStep(-1);
            emit errorOccurred("Connection failed");
        }
        else if (statusMinor == "Client disconnecting") {
            if (afterStatus.contains(": Connection timeout")) {
                setCurrentConnectionStep(-1);
                emit errorOccurred("The server could not be reached or it failed to respond. Common causes are :\n"
                                 "i. You are already on IITD network. Please connect externally.\n"
                                 "ii. You are behind a firewall blocking port 1194. Please try to connect with your mobile hotspot instead.");
            }
        }
        else if (statusMinor == "Client disconnected") {
            setCurrentConnectionStep(-1);
            connectedState = VpnConnectionState::Disconnected;
            emit disconnected();
            emit connectionStateChanged(connectedState);
            m_statsTimer.stop();
            m_statsProcess.kill();
        }
        else if (statusMinor == "Client process exited") {
            setCurrentConnectionStep(-1);
            m_statsTimer.stop();
            m_statsProcess.kill();
        }
    }
}
