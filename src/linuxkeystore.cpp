#include "linuxkeystore.h"

#ifdef Q_OS_LINUX

#include <QProcess>
#include <QDebug>
#include <QTemporaryFile>

namespace {
    const QString kSecretTool = QStringLiteral("secret-tool");
}

LinuxKeyStore::LinuxKeyStore(QObject *parent)
    : IKeyStore(parent)
{
}

QByteArray LinuxKeyStore::runCommand(const QString &program, const QStringList &arguments, const QByteArray &inputData) const
{
    QProcess proc;
    proc.start(program, arguments);

    if (!inputData.isEmpty()) {
        proc.write(inputData);
        proc.closeWriteChannel();
    }

    if (!proc.waitForFinished(5000)) {
        qWarning() << "[LinuxKeyStore]" << program << "timed out.";
        proc.kill();
        return {};
    }

    if (proc.exitCode() != 0) {
        qWarning() << "[LinuxKeyStore]" << program << "failed with exit code" << proc.exitCode() << "stderr:" << proc.readAllStandardError();
        return {};
    }

    return proc.readAllStandardOutput();
}

QByteArray LinuxKeyStore::retrieveKey() const
{
    return runCommand(kSecretTool, {
        QStringLiteral("lookup"),
        QStringLiteral("application"), QStringLiteral("iitdvpn"),
        QStringLiteral("key"), QStringLiteral("private-key")
    });
}

bool LinuxKeyStore::storeKey(const QByteArray &keyData) const
{
    QByteArray output = runCommand(kSecretTool, {
        QStringLiteral("store"),
        QStringLiteral("--label=") + QLatin1String(IKeyStore::kKeyName),
        QStringLiteral("application"), QStringLiteral("iitdvpn"),
        QStringLiteral("key"), QStringLiteral("private-key")
    }, keyData);

    return !output.isNull();
}

void LinuxKeyStore::clearStoredKey() const
{
    runCommand(kSecretTool, {
        QStringLiteral("clear"),
        QStringLiteral("application"), QStringLiteral("iitdvpn"),
        QStringLiteral("key"), QStringLiteral("private-key")
    });
}

bool LinuxKeyStore::checkKeyExists()
{
    QByteArray keyData = retrieveKey();
    if (keyData.isEmpty()) {
        return false;
    }

    QByteArray output = runCommand(QStringLiteral("openssl"), {
        QStringLiteral("ec"), QStringLiteral("-noout")
    }, keyData);

    return !output.isEmpty();
}

void LinuxKeyStore::generateKey()
{
    QByteArray keyData = runCommand(QStringLiteral("openssl"), {
        QStringLiteral("ecparam"),
        QStringLiteral("-name"), QStringLiteral("prime256v1"),
        QStringLiteral("-genkey"),
        QStringLiteral("-noout")
    });

    if (keyData.isEmpty()) {
        qWarning() << "[LinuxKeyStore] Key generation failed.";
        return;
    }

    if (storeKey(keyData)) {
        qDebug() << "[LinuxKeyStore] ECDSA P-256 key generated and stored in secret service.";
    } else {
        qWarning() << "[LinuxKeyStore] Key generated but failed to store in secret service.";
    }
}

QByteArray LinuxKeyStore::signData(const QByteArray &data)
{
    QByteArray keyData = retrieveKey();
    if (keyData.isEmpty()) {
        qWarning() << "[LinuxKeyStore] No key available for signing.";
        return {};
    }

    QTemporaryFile tmpKey;
    if (!tmpKey.open()) {
        qWarning() << "[LinuxKeyStore] Failed to create temporary file for signing.";
        return {};
    }
    tmpKey.write(keyData);
    tmpKey.flush();

    QByteArray result = runCommand(QStringLiteral("openssl"), {
        QStringLiteral("pkeyutl"),
        QStringLiteral("-sign"),
        QStringLiteral("-inkey"), tmpKey.fileName(),
        QStringLiteral("-pkeyopt"), QStringLiteral("digest:sha256")
    }, data);

    tmpKey.remove();
    return result;
}

QByteArray LinuxKeyStore::generateCsr(const QString &username, const QString &email)
{
    QByteArray keyData = retrieveKey();
    if (keyData.isEmpty()) {
        qWarning() << "[LinuxKeyStore] No key available for CSR generation.";
        return {};
    }

    QTemporaryFile tmpKey;
    if (!tmpKey.open()) {
        qWarning() << "[LinuxKeyStore] Failed to create temporary file for CSR generation.";
        return {};
    }
    tmpKey.write(keyData);
    tmpKey.flush();

    QString subj = QString(QStringLiteral("/C=IN/ST=Delhi/L=New Delhi/") +
                           QStringLiteral("O=Indian Institute of Technology, Delhi/") +
                           QStringLiteral("OU=Computer Services Center, IITD/") +
                           QStringLiteral("CN=%1/emailAddress=%2")).arg(username, email);

    QByteArray result = runCommand(QStringLiteral("openssl"), {
        QStringLiteral("req"),
        QStringLiteral("-new"),
        QStringLiteral("-key"), tmpKey.fileName(),
        QStringLiteral("-subj"), subj
    });

    tmpKey.remove();
    return result;
}

void LinuxKeyStore::clearKey()
{
    clearStoredKey();
    qDebug() << "[LinuxKeyStore] Key deleted from secret service.";
}

#endif