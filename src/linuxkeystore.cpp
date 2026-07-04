#include "linuxkeystore.h"

#include <QDebug>

LinuxKeyStore::LinuxKeyStore(QObject *parent)
    : IKeyStore(parent)
{
}

bool LinuxKeyStore::checkKeyExists()
{
    qWarning() << "[LinuxKeyStore] checkKeyExists: not yet implemented.";
    return false;
}

void LinuxKeyStore::generateKey()
{
    qWarning() << "[LinuxKeyStore] generateKey: not yet implemented.";
}

QByteArray LinuxKeyStore::signData(const QByteArray &data)
{
    Q_UNUSED(data);
    qWarning() << "[LinuxKeyStore] signData: not yet implemented.";
    return {};
}

QByteArray LinuxKeyStore::generateCsr(const QString &username,
                                      const QString &email)
{
    Q_UNUSED(username);
    Q_UNUSED(email);
    qWarning() << "[LinuxKeyStore] generateCsr: not yet implemented.";
    return {};
}

void LinuxKeyStore::clearKey()
{
    qWarning() << "[LinuxKeyStore] clearKey: not yet implemented.";
}
