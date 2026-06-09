#ifndef VPNCONTROLLER_H
#define VPNCONTROLLER_H

#include <QObject>
#include <memory>
#include "ivpnbackend.h"

class VpnController : public QObject
{
    Q_OBJECT
public:
    explicit VpnController(QObject *parent = nullptr);

    void connectVpn(const QString &ovpnPath,
                    const QString &password);

    void updatePassword(const QString &password);

    void disconnectVpn();
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);
    void logLineReceived(const QString &line);
    void statusChanged(const QString &status);
    void stateChanged(const QString &state);
    void byteCountChanged(qulonglong uploadBytes, qulonglong downloadBytes);
    void connectionInfoChanged(const QString &remote, const QString &remoteAddr, const QString &proto, const QString &localIface, const QString &localIp, const QString &gateway, int mtu);

private:
    std::unique_ptr<IVpnBackend> backend;
};

#endif
