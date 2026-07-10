#include "linuxkeystore.h"

#ifdef Q_OS_LINUX

#include <QProcess>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTemporaryFile>

LinuxKeyStore::LinuxKeyStore(QObject *parent)
    : IKeyStore(parent)
{
}

QString LinuxKeyStore::getKeyFilePath() const
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configPath); 
    return configPath + QStringLiteral("/pkey.pem");
}

bool LinuxKeyStore::storeKey(const QByteArray &keyData) const
{
    QFile file(getKeyFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[LinuxKeyStore] Failed to open key file for writing.";
        return false;
    }

    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    
    file.write(keyData);
    return true;
}

QByteArray LinuxKeyStore::retrieveKey() const
{
    QFile file(getKeyFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
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
        return QByteArray();
    }

    if (proc.exitCode() != 0) {
        qWarning() << "[LinuxKeyStore]" << program << "failed with exit code" << proc.exitCode() 
                   << "stderr:" << proc.readAllStandardError();
        return QByteArray();
    }

    return proc.readAllStandardOutput();
}

bool LinuxKeyStore::checkKeyExists()
{
    QByteArray keyData = retrieveKey();
    if (keyData.isEmpty()) {
        return false;
    }

    QByteArray output = runCommand(QStringLiteral("openssl"), {
        QStringLiteral("ec")
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
        qWarning() << "[LinuxKeyStore] OpenSSL Elliptic Curve Key generation failed.";
        return;
    }

    if (storeKey(keyData)) {
        qDebug() << "[LinuxKeyStore] ECDSA P-256 key successfully generated and stored locally.";
    }
}

QByteArray LinuxKeyStore::signData(const QByteArray &data)
{
    QByteArray keyData = retrieveKey();
    if (keyData.isEmpty()) {
        qWarning() << "[LinuxKeyStore] No key available for signing.";
        return QByteArray();
    }

    QTemporaryFile tmpKey;
    if (!tmpKey.open()) {
        qWarning() << "[LinuxKeyStore] Failed to generate safe temporary file context for signing.";
        return QByteArray();
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
        return QByteArray();
    }

    QTemporaryFile tmpKey;
    if (!tmpKey.open()) {
        qWarning() << "[LinuxKeyStore] Failed to generate safe temporary file context for CSR.";
        return QByteArray();
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
    QFile::remove(getKeyFilePath());
    qDebug() << "[LinuxKeyStore] Key explicitly wiped from filesystem storage.";
}

#endif