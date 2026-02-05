#ifndef VPNCONTROLLER_H
#define VPNCONTROLLER_H

#include <QObject>
#include <QProcess>
#include <QDir>
#include <QCoreApplication>
#include "openvpnmgmt.h"

class VpnController : public QObject
{
    Q_OBJECT

public:
    explicit VpnController(QObject *parent = nullptr);

    void connectVpn(const QString &ovpnPath,
                    const QString &username,
                    const QString &password);

    void disconnectVpn();
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);
    void statusChanged(const QString &status);

private:
    QProcess *vpnProcess = nullptr;
    OpenVpnMgmt *mgmt = nullptr;
    bool connectedState = false;

    QString resolveOpenVpnBinary() const;
};

#endif
