#pragma once

#include "ikeystore.h"
#include <QByteArray>
#include <QString>
#include <QStringList>

#ifdef Q_OS_LINUX

class LinuxKeyStore : public IKeyStore
{
    Q_OBJECT

public:
    explicit LinuxKeyStore(QObject *parent = nullptr);
    ~LinuxKeyStore() override = default;

    bool checkKeyExists() override;
    void generateKey() override;
    QByteArray signData(const QByteArray &data) override;
    QByteArray generateCsr(const QString &username, const QString &email) override;
    void clearKey() override;

private:
    QString getKeyFilePath() const;
    bool storeKey(const QByteArray &keyData) const;
    QByteArray retrieveKey() const;
    
    QByteArray runCommand(const QString &program, 
                          const QStringList &arguments, 
                          const QByteArray &inputData = QByteArray()) const;
};

#endif