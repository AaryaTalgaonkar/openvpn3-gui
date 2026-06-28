#pragma once

#include "ikeystore.h"

#ifdef Q_OS_WIN

#include <QByteArray>
#include <QString>

class WindowsKeyStore : public IKeyStore
{
    Q_OBJECT

public:
    explicit WindowsKeyStore(QObject *parent = nullptr);

    bool checkKeyExists() override;
    void generateKey() override;
    QByteArray signData(const QByteArray &data) override;
    QByteArray generateCsr(const QString &username,
                           const QString &email) override;
    void clearKey() override;

private:
    QByteArray runPowerShell(const QString &script,
                             int timeoutMs = 60000) const;

    static QString providerParam();
};

#endif 
