#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

class IKeyStore : public QObject
{
    Q_OBJECT

public:
    explicit IKeyStore(QObject *parent = nullptr)
        : QObject(parent) {}

    virtual ~IKeyStore() = default;

    static constexpr const char *kKeyName = "TestVPNKey";

    virtual bool checkKeyExists() = 0;

    virtual void generateKey() = 0;

    virtual QByteArray signData(const QByteArray &data) = 0;

    virtual QByteArray generateCsr(const QString &username,
                                   const QString &email) = 0;

    virtual void clearKey() = 0;
};
