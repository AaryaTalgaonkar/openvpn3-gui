#ifndef IVPNBACKEND_H
#define IVPNBACKEND_H

#include <QObject>

enum class VpnConnectionState {
    Disconnected,
    Connecting,
    Connected
};

struct ConnectionStepDefinition {
    const char *state;
    const char *icon;
    const char *label;
};

class IVpnBackend : public QObject
{
    Q_OBJECT
public:
    explicit IVpnBackend(QObject *parent = nullptr)
        : QObject(parent) {}

    virtual ~IVpnBackend() = default;

    virtual void connectVpn(const QString &ovpnPath,
                            const QString &password) = 0;

    virtual void updatePassword(const QString &password) = 0;

    virtual void disconnectVpn() = 0;
    virtual VpnConnectionState connectionState() const = 0;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);
    void logLineReceived(const QString &line);
    void statusChanged(const QString &status);
    void stateChanged(const QString &state);
    void byteCountChanged(qulonglong uploadBytes, qulonglong downloadBytes);
    void connectionInfoChanged(const QString &remote, const QString &remoteAddr, const QString &proto, const QString &localIface, const QString &localIp, const QString &gateway, int mtu);
};

#endif
