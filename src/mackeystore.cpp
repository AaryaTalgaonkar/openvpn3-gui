#include "mackeystore.h"

#include <QDebug>

MacKeyStore::MacKeyStore(QObject *parent)
    : IKeyStore(parent)
{
}

bool MacKeyStore::checkKeyExists()
{
    // TODO: Implement using Security framework (SecKeychain / SecItem).
    qWarning() << "[MacKeyStore] checkKeyExists: not yet implemented.";
    return false;
}

void MacKeyStore::generateKey()
{
    qWarning() << "[MacKeyStore] generateKey: not yet implemented.";
}

QByteArray MacKeyStore::signData(const QByteArray &data)
{
    Q_UNUSED(data);
    qWarning() << "[MacKeyStore] signData: not yet implemented.";
    return {};
}

QByteArray MacKeyStore::generateCsr(const QString &username,
                                    const QString &email)
{
    Q_UNUSED(username);
    Q_UNUSED(email);
    qWarning() << "[MacKeyStore] generateCsr: not yet implemented.";
    return {};
}

void MacKeyStore::clearKey()
{
    qWarning() << "[MacKeyStore] clearKey: not yet implemented.";
}
