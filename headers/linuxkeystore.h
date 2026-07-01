#pragma once

#include "ikeystore.h"

#include <QByteArray>
#include <QString>

class LinuxKeyStore : public IKeyStore
{
    Q_OBJECT

public:
    explicit LinuxKeyStore(QObject *parent = nullptr);

    bool checkKeyExists() override;
    void generateKey() override;
    QByteArray signData(const QByteArray &data) override;
    QByteArray generateCsr(const QString &username,
                           const QString &email) override;
    void clearKey() override;
};
