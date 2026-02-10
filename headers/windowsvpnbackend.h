#ifndef WINDOWSVPNBACKEND_H
#define WINDOWSVPNBACKEND_H

#include "ivpnbackend.h"
#include "openvpnmgmt.h"

#include <QProcess>
#include <QDir>
#include <QCoreApplication>

class WindowsVpnBackend : public IVpnBackend
{
    Q_OBJECT

public:
    explicit WindowsVpnBackend(QObject *parent = nullptr);

    void connectVpn(const QString &ovpnPath,
                    const QString &username,
                    const QString &password) override;

    void disconnectVpn() override;
    bool isConnected() const override;

private:
    QProcess *vpnProcess = nullptr;
    OpenVpnMgmt *mgmt = nullptr;
    bool connectedState = false;

    QString resolveOpenVpnBinary() const;
};

#endif
