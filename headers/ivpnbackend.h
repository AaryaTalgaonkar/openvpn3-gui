#ifndef IVPNBACKEND_H
#define IVPNBACKEND_H

#include <QObject>

class IVpnBackend : public QObject
{
    Q_OBJECT
public:
    explicit IVpnBackend(QObject *parent = nullptr)
        : QObject(parent) {}

    virtual ~IVpnBackend() = default;

    virtual void connectVpn(const QString &ovpnPath,
                            const QString &username,
                            const QString &password) = 0;

    virtual void disconnectVpn() = 0;
    virtual bool isConnected() const = 0;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);
    void statusChanged(const QString &status);
};

#endif
