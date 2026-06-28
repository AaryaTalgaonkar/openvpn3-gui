#include "windowskeystore.h"

#ifdef Q_OS_WIN

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>

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
        "$prov = New-Object System.Security.Cryptography.CngProvider(\"%1\");\n"
        "try {\n"
        "    $key = [System.Security.Cryptography.CngKey]::Open(\"%2\", $prov);\n"
        "    $key.Dispose();\n"
        "    Write-Output \"true\";\n"
        "} catch {\n"
        "    Write-Output \"false\";\n"
        "}\n"
    ).arg(providerParam(), QLatin1String(kKeyName));

    const QByteArray output = runPowerShell(script);
    return output.trimmed().toLower() == "true";
}

void WindowsKeyStore::generateKey()
{
    const QString script = QStringLiteral(
        "$prov = New-Object System.Security.Cryptography.CngProvider(\"%1\");\n"
        "$params = New-Object System.Security.Cryptography.CngKeyCreationParameters;\n"
        "$params.Provider = $prov;\n"
        "$params.KeyCreationOptions = [System.Security.Cryptography.CngKeyCreationOptions]::OverwriteExistingKey;\n"
        "$key = [System.Security.Cryptography.CngKey]::Create(\n"
        "    [System.Security.Cryptography.CngAlgorithm]::ECDsaP256,\n"
        "    \"%2\",\n"
        "    $params\n"
        ");\n"
        "$key.Dispose();\n"
        "Write-Output \"OK\";\n"
    ).arg(providerParam(), QLatin1String(kKeyName));

    const QByteArray output = runPowerShell(script);
    if (output.trimmed() == "OK") {
        qDebug() << "[WindowsKeyStore] ECDSA P-256 key generated successfully.";
    } else {
        qWarning() << "[WindowsKeyStore] Key generation may have failed."
                    << "Output:" << output;
    }
}

QByteArray WindowsKeyStore::signData(const QByteArray &data)
{
    const QString b64Data = QString::fromLatin1(data.toBase64());

    const QString script = QStringLiteral(
        "$prov = New-Object System.Security.Cryptography.CngProvider(\"%1\");\n"
        "$key = [System.Security.Cryptography.CngKey]::Open(\"%2\", $prov);\n"
        "$signer = New-Object System.Security.Cryptography.ECDsaCng($key);\n"
        "$data = [Convert]::FromBase64String(\"%3\");\n"
        "$sig = $signer.SignData($data, [System.Security.Cryptography.HashAlgorithmName]::SHA256);\n"
        "$key.Dispose();\n"
        "Write-Output ([Convert]::ToBase64String($sig));\n"
    ).arg(providerParam(), QLatin1String(kKeyName), b64Data);

    const QByteArray output = runPowerShell(script).trimmed();
    if (output.isEmpty()) {
        qWarning() << "[WindowsKeyStore] signData: received empty signature.";
        return {};
    }

    return QByteArray::fromBase64(output);
}

QByteArray WindowsKeyStore::generateCsr(const QString &username,
                                        const QString &email)
{
    const QString safeUser  = QString(username).replace(QChar('\"'), QStringLiteral("\"\""));
    const QString safeEmail = QString(email).replace(QChar('\"'), QStringLiteral("\"\""));

    const QString infContent = QStringLiteral(
        "[Version]\n"
        "Signature=\"$Windows NT$\"\n"
        "\n"
        "[NewRequest]\n"
        "Subject = \"C=IN, S=Delhi, L=New Delhi, "
        "O=\"\"Indian Institute of Technology, Delhi\"\", "
        "OU=\"\"Computer Services Center, IITD\"\", "
        "CN=%1, E=%2\"\n"
        "\n"
        "ProviderName = \"%3\"\n"
        "KeyContainer = \"%4\"\n"
        "UseExistingKeySet = True\n"
        "\n"
        "KeyAlgorithm = ECDSA_P256\n"
        "HashAlgorithm = SHA256\n"
        "\n"
        "RequestType = PKCS10\n"
        "\n"
        "KeyUsage = 0x80\n"
    ).arg(safeUser, safeEmail, providerParam(), QLatin1String(kKeyName));

    QTemporaryDir tmpDir;
    if (!tmpDir.isValid()) {
        qWarning() << "[WindowsKeyStore] generateCsr: failed to create temp dir.";
        return {};
    }

    const QString infPath = tmpDir.filePath(QStringLiteral("request.inf"));
    const QString csrPath = tmpDir.filePath(QStringLiteral("request.csr"));

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
                      infPath,
                      csrPath
                  });

    const bool finished = certReq.waitForFinished(60000);
    const QByteArray csrStdErr = certReq.readAllStandardError();

    if (!finished || certReq.exitCode() != 0) {
        qWarning() << "[WindowsKeyStore] generateCsr: certreq.exe failed."
                    << "Exit code:" << certReq.exitCode()
                    << "Stderr:" << csrStdErr;
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

    return csrData;
}

void WindowsKeyStore::clearKey()
{
    const QString script = QStringLiteral(
        "$prov = New-Object System.Security.Cryptography.CngProvider(\"%1\");\n"
        "$key = [System.Security.Cryptography.CngKey]::Open(\"%2\", $prov);\n"
        "$key.Delete();\n"
        "Write-Output \"OK\";\n"
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

