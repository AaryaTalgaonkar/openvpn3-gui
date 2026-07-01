#pragma once

#include "ikeystore.h"

#include <QByteArray>
#include <QString>

/// Stub implementation for macOS.
/// TODO: Implement using the Security framework / Keychain when required.
class MacKeyStore : public IKeyStore
{
    Q_OBJECT

public:
    explicit MacKeyStore(QObject *parent = nullptr);

    bool checkKeyExists() override;
    void generateKey() override;
    QByteArray signData(const QByteArray &data) override;
    QByteArray generateCsr(const QString &username,
                           const QString &email) override;
    void clearKey() override;
};
