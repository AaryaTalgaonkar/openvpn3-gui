#ifndef OPENVPNMGMT_H
#define OPENVPNMGMT_H

#include <QObject>
#include <QTcpSocket>
#include <QDebug>
#include <QTimer>

class OpenVpnMgmt : public QObject
{
    Q_OBJECT

public:
    explicit OpenVpnMgmt(const QString &host, quint16 port, QObject *parent = nullptr);

    void start(const QString &username, const QString &password);
    void terminate();

signals:
    void connected();
    void authFailed(const QString &reason);
    void fatalError(const QString &reason);

private slots:
    void onReadyRead();

private:
    QTcpSocket socket;
    QString user;
    QString pass;
    QString host;
    quint16 port;

};

#endif
