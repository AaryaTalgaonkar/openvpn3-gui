#include "windowskeystore.h"

#ifdef Q_OS_WIN

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

#include <windows.h>
#include <wincrypt.h>

QString WindowsKeyStore::providerParam()
{
    return QStringLiteral("Microsoft Platform Crypto Provider");
}

QByteArray WindowsKeyStore::runPowerShell(const QString &script,
                                          int timeoutMs) const
{
    QProcess proc;
    proc.start(QStringLiteral("powershell.exe"),
               {
                   QStringLiteral("-NoProfile"),
                   QStringLiteral("-NonInteractive"),
                   QStringLiteral("-Command"),
                   script
               });

    const bool finished = proc.waitForFinished(timeoutMs);
    const QByteArray stdOut = proc.readAllStandardOutput();
    const QByteArray stdErr = proc.readAllStandardError();

    if (!finished) {
        qWarning() << "[WindowsKeyStore] PowerShell timed out after"
                    << timeoutMs << "ms";
        proc.kill();
        proc.waitForFinished(5000);
        return {};
    }

    if (proc.exitCode() != 0) {
        qWarning() << "[WindowsKeyStore] PowerShell exited with code"
                    << proc.exitCode() << "stderr:" << stdErr;
        return {};
    }

    if (!stdErr.isEmpty()) {
        qDebug() << "[WindowsKeyStore] PowerShell stderr:" << stdErr;
    }

    return stdOut;
}

WindowsKeyStore::WindowsKeyStore(QObject *parent)
    : IKeyStore(parent)
{
}

bool WindowsKeyStore::checkKeyExists()
{
    const QString script = QStringLiteral(
        "$prov = New-Object System.Security.Cryptography.CngProvider(\"%1\");"
        "try {"
        "    $key = [System.Security.Cryptography.CngKey]::Open(\"%2\", $prov);"
        "    $key.Dispose();"
        "    Write-Output \"true\";"
        "} catch {"
        "    Write-Output \"false\";"
        "}"
    ).arg(providerParam(), QLatin1String(kKeyName));

    const QByteArray output = runPowerShell(script);
    return output.trimmed().toLower() == "true";
}

void WindowsKeyStore::generateKey()
{
    const QString script = QStringLiteral(
        "$prov = New-Object System.Security.Cryptography.CngProvider(\"%1\");"
        "$params = New-Object System.Security.Cryptography.CngKeyCreationParameters;"
        "$params.Provider = $prov;"
        "$params.KeyCreationOptions = [System.Security.Cryptography.CngKeyCreationOptions]::OverwriteExistingKey;"
        "$key = [System.Security.Cryptography.CngKey]::Create("
        "    [System.Security.Cryptography.CngAlgorithm]::ECDsaP256,"
        "    \"%2\","
        "    $params"
        ");"
        "$key.Dispose();"
        "Write-Output \"OK\";"
    ).arg(providerParam(), QLatin1String(kKeyName));

    const QByteArray output = runPowerShell(script);
    if (output.trimmed() == "OK") {
        qDebug() << "[WindowsKeyStore] ECDSA P-256 key generated successfully.";
    } else {
        qWarning() << "[WindowsKeyStore] Key generation may have failed."
                    << "Output:" << output;
    }
}
QByteArray WindowsKeyStore::p1363ToDer(const QByteArray &p1363Sig)
{

    if (p1363Sig.size() != 64) {
        qWarning() << "[p1363ToDer] Invalid P1363 signature length:" << p1363Sig.size()
                << "expected 64 bytes";
        return {};
    }

    QByteArray rPart = p1363Sig.left(32);
    QByteArray sPart = p1363Sig.mid(32);

    CERT_ECC_SIGNATURE eccSig = {};

    QByteArray rLe = rPart;
    QByteArray sLe = sPart;

    std::reverse(rLe.begin(), rLe.end());
    std::reverse(sLe.begin(), sLe.end());

    eccSig.r.cbData = rLe.size();
    eccSig.r.pbData = reinterpret_cast<BYTE*>(rLe.data());

    eccSig.s.cbData = sLe.size();
    eccSig.s.pbData = reinterpret_cast<BYTE*>(sLe.data());

    DWORD pcbEncoded = 0;

    if (!CryptEncodeObjectEx(X509_ASN_ENCODING, X509_ECC_SIGNATURE, &eccSig, 0, nullptr, nullptr, &pcbEncoded)) {
        DWORD err = GetLastError();
        qWarning() << "[p1363ToDer] CryptEncodeObjectEx sizing FAILED. Error:" << QString::number(err, 16);
        return {};
    }

    QByteArray derSig(static_cast<int>(pcbEncoded), Qt::Uninitialized);

    if (!CryptEncodeObjectEx(X509_ASN_ENCODING, X509_ECC_SIGNATURE, &eccSig, 0, nullptr, reinterpret_cast<BYTE*>(derSig.data()), &pcbEncoded)) {
        DWORD err = GetLastError();
        qWarning() << "[p1363ToDer] CryptEncodeObjectEx encoding FAILED. Error:" << QString::number(err, 16);
        return {};
    }

    return derSig;
}

QByteArray WindowsKeyStore::signData(const QByteArray &data)
{
    const QString b64Data = QString::fromLatin1(data.toBase64());

    const QString script = QStringLiteral(
        "$prov = New-Object System.Security.Cryptography.CngProvider(\"%1\");"
        "$key = [System.Security.Cryptography.CngKey]::Open(\"%2\", $prov);"
        "$signer = New-Object System.Security.Cryptography.ECDsaCng($key);"
        "$data = [Convert]::FromBase64String(\"%3\");"
        "$sig = $signer.SignData($data, [System.Security.Cryptography.HashAlgorithmName]::SHA256);"
        "$key.Dispose();"
        "Write-Output ([Convert]::ToBase64String($sig));"
    ).arg(providerParam(), QLatin1String("IITDelhiVPNKey"), b64Data);

    const QByteArray output = runPowerShell(script).trimmed();
    if (output.isEmpty()) {
        qWarning() << "[WindowsKeyStore] signData: received empty signature.";
        return {};
    }

    const QByteArray p1363Sig = QByteArray::fromBase64(output);

    const QByteArray derSig = p1363ToDer(p1363Sig);

    return derSig;
}

QByteArray WindowsKeyStore::generateCsr(const QString &username,
                                        const QString &email)
{
    const QString safeUser  = QString(username).replace(QChar('\"'), QStringLiteral("\"\""));
    const QString safeEmail = QString(email).replace(QChar('\"'), QStringLiteral("\"\""));

    const QString infContent = QStringLiteral(
        "[Version]\n"
        "Signature=\"$Windows NT$\"\n"
        "[NewRequest]\n"
        "Subject = \"C=IN, S=Delhi, L=New Delhi, "
        "O=\"\"Indian Institute of Technology, Delhi\"\", "
        "OU=\"\"Computer Services Center, IITD\"\", "
        "CN=%1, E=%2\"\n"
        "ProviderName = \"%3\"\n"
        "KeyContainer = \"%4\"\n"
        "UseExistingKeySet = True\n"
        "KeyAlgorithm = ECDSA_P256\n"
        "HashAlgorithm = SHA256\n"
        "RequestType = PKCS10\n"
        "KeyUsage = 0x80 ; Digital Signature\n"
    ).arg(safeUser, safeEmail, providerParam(), QLatin1String(kKeyName));

    QDir baseDir;
    {
        const QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (basePath.isEmpty()) {
            qWarning() << "[WindowsKeyStore] generateCsr: cannot get app data path.";
            return {};
        }
        baseDir = QDir(basePath);
    }
    if (!baseDir.mkpath("configurations")) {
        qWarning() << "[WindowsKeyStore] generateCsr: cannot create config dir.";
        return {};
    }

    const QString stableDir = baseDir.filePath(QStringLiteral("configurations"));
    const QString infPath = QDir(stableDir).filePath(QStringLiteral("request.inf"));
    const QString csrPath = QDir(stableDir).filePath(QStringLiteral("request.csr"));

    QFile::remove(infPath);
    QFile::remove(csrPath);

    {
        QFile infFile(infPath);
        if (!infFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "[WindowsKeyStore] generateCsr: cannot write INF at"
                        << infPath;
            return {};
        }
        QTextStream out(&infFile);
        out << infContent;
        infFile.close();
    }

    QProcess certReq;
    certReq.start(QStringLiteral("certreq.exe"),
                {
                    QStringLiteral("-new"),
                    QDir::toNativeSeparators(infPath), 
                    QDir::toNativeSeparators(csrPath) 
                });

    const bool finished = certReq.waitForFinished(60000);
    const QByteArray csrStdErr = certReq.readAllStandardError();

    QFile::remove(infPath);

    if (!finished || certReq.exitCode() != 0) {
        qWarning() << "[WindowsKeyStore] generateCsr: certreq.exe failed."
                    << "Exit code:" << certReq.exitCode()
                    << "Stderr:" << csrStdErr;
        QFile::remove(csrPath);
        return {};
    }

    QFile csrFile(csrPath);
    if (!csrFile.open(QIODevice::ReadOnly)) {
        qWarning() << "[WindowsKeyStore] generateCsr: cannot read CSR at"
                    << csrPath;
        return {};
    }

    const QByteArray csrData = csrFile.readAll();
    csrFile.close();

    QFile::remove(csrPath);

    return csrData;
}

void WindowsKeyStore::clearKey()
{
    const QString script = QStringLiteral(
        "$prov = New-Object System.Security.Cryptography.CngProvider(\"%1\");"
        "$key = [System.Security.Cryptography.CngKey]::Open(\"%2\", $prov);"
        "$key.Delete();"
        "Write-Output \"OK\";"
    ).arg(providerParam(), QLatin1String(kKeyName));

    const QByteArray output = runPowerShell(script);
    if (output.trimmed() == "OK") {
        qDebug() << "[WindowsKeyStore] Key deleted successfully.";
    } else {
        qWarning() << "[WindowsKeyStore] clearKey may have failed."
                    << "Output:" << output;
    }
}

#endif 

