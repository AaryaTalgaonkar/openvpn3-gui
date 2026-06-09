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

    /// Returns the step definitions for this platform
    virtual const ConnectionStepDefinition *connectionSteps() const { return nullptr; }
    virtual int connectionStepCount() const { return 0; }

    /// Returns the index of the current connection step (-1 if not in a stepped phase)
    virtual int currentConnectionStepIndex() const { return -1; }

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);
    void logLineReceived(const QString &line);
    void connectionStateChanged(VpnConnectionState state);
    void stateChanged(const QString &state);
    void byteCountChanged(qulonglong uploadBytes, qulonglong downloadBytes);
    void connectionInfoChanged(const QString &remote, const QString &remoteAddr, const QString &proto, const QString &localIface, const QString &localIp, const QString &gateway, int mtu);

    /// Emitted when the connection step advances. stepIndex is the index into connectionSteps(),
    /// or -1 when no step is active (e.g. before connecting or after fully connected).
    void connectionStepChanged(int stepIndex);
};

#endif