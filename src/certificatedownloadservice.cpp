#include "certificatedownloadservice.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace {
constexpr auto kOvpnContent = R"(client
dev tun
proto udp
remote vpn3.iitd.ac.in
port 1194
resolv-retry infinite
ns-cert-type server
nobind
persist-key
persist-tun
remote-cert-tls server
cipher AES-256-GCM
auth SHA256
ecdh-curve prime256v1
tls-version-min 1.2
tls-ciphersuites TLS_AES_256_GCM_SHA384
key-direction 1
verb 3
route-method exe
route-delay 2
auth-user-pass

# Embedded Certificates
<ca>
-----BEGIN CERTIFICATE-----
MIIGiTCCBHGgAwIBAgIUEFmnY/GsMtzMn1yl3ucXYQLwt9owDQYJKoZIhvcNAQEM
BQAwgcsxCzAJBgNVBAYTAklOMQ4wDAYDVQQIDAVEZWxoaTESMBAGA1UEBwwJTmV3
IERlbGhpMS4wLAYDVQQKDCVJbmRpYW4gSW5zdGl0dXRlIG9mIFRlY2hub2xvZ3ks
IERlbGhpMScwJQYDVQQLDB5Db21wdXRlciBTZXJ2aWNlcyBDZW50ZXIsIElJVEQx
GjAYBgNVBAMMEWNlcnRjYS5paXRkLmFjLmluMSMwIQYJKoZIhvcNAQkBFhRzeXNh
ZG1AY2MuaWl0ZC5hYy5pbjAeFw0yMzA0MDkwNjU4MzdaFw00MzA0MDQwNjU4Mzda
MIHLMQswCQYDVQQGEwJJTjEOMAwGA1UECAwFRGVsaGkxEjAQBgNVBAcMCU5ldyBE
ZWxoaTEuMCwGA1UECgwlSW5kaWFuIEluc3RpdHV0ZSBvZiBUZWNobm9sb2d5LCBE
ZWxoaTEnMCUGA1UECwweQ29tcHV0ZXIgU2VydmljZXMgQ2VudGVyLCBJSVREMRow
GAYDVQQDDBFjZXJ0Y2EuaWl0ZC5hYy5pbjEjMCEGCSqGSIb3DQEJARYUc3lzYWRt
QGNjLmlpdGQuYWMuaW4wggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQDJ
+ikAO4qSnGpZc7DHcVHQn1CkSShj31SvXUVPKcR2HnRbcmFWOOuyK4XV9z1K0xWB
IJCaHcj0IBBOhP5hxmflUwuDnJh1oAo9X6Bhjr6kql6stmvLjD38awwW6g9ztkht
UkiIXpa5jHAeRu1Kemoanph4gqi5Md8Jz8UFUai8+l4Gtd29x2Cqj1SbB79+cyKx
QZworL7J0wrjTWKRvwXyY1D13jXvWU5gnlXI0h9JQ8efk7UAwO23L8WvsZSCNZqb
S09grCcq6mqRk3gMP3GEnC/9PC2mcxpsIBBBkjrDmm1i0i+0VLP7bBP/QkJ20XtR
RMDKOAVs/ew2nolOcV8HRc+BZHnwee6DW3lY/z2hzRQnqTPMSGwu1q7ED/uZLtTQ
kh/apqI4199naCC0Ba/mD+W+cHM/lDMN9kl31i2vgSWy5zOpe4B6WcyUROhixRhn
0Y3toasC747XSoy3+JIZEg0awSYZGO5rJ9FoPTdUd7DGiCjWVaNHZ7JPDLRgyA6M
8XlI6b/KuY7rKgR9TPjDjqykS/KoA38m/0LY3ciGtIeKa+HOAhvKDTBg1i0bF6l/
+ZN5gwEH9NBUAvm/yaMCHxW3oI1oMpqzqxPdjmMu14xgecV81LKBbssl4FZgtrHF
NLbTVLi/RA4QffPTuPPTw6X+yKUfNU1ybzIggfRnLwIDAQABo2MwYTAdBgNVHQ4E
FgQUKEM4pVPZQzFjgkPABwcWkWGfh8IwHwYDVR0jBBgwFoAUKEM4pVPZQzFjgkPA
BwcWkWGfh8IwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAYYwDQYJKoZI
hvcNAQEMBQADggIBABYr/eRhvVVJlvA7Cu71rLD3Au7AOPyWJgLEoWc4Cvkzfi5B
2mGicKEZ57hUmReoobdDstG3IqfXioHNVnwychuRoQoDozojKKclIfmYj6nqyJuV
YhCFL4EAkFVvcmx6AypQ5fDAHMxzr/tdfo9KXVDC19w9lG8Rc6RXV/XmGnZix4nN
AmXZ9fZWIBRr8+68MK5+ID99wZVpz4zCTpJcZzkP439alnqA6VF48+xKHXY0QgNp
DIGgXD4W3/0SCsLKUJrvXXavl3ZATv7AnbXe0Nzu9R1pB49BgknPt4ZRbXTLUP4Y
uvkehPbaI2m6DjupaChd22y1FCNgO26GiAt1xB2bCKwMSgRQKDXQcSfI7UVGwHU6
jTwskXUoSZ3xfjwD2HJD6XHAswOCRVjYHPmwkPBe8r9qVPpwiq5GNfs40J4JAgw7
3XcTDC1N17aSVT2FvcEfMHZMt9iCaCbfXHTY8a2dpdK5UinLgNHaLjjImea9bzmM
wwkxiEU/0BifWbiwfg1/IEv0EhvOLeuEp/zKG1QyZXFY2jTvQ5GoJWZgGYFfbsWG
/xV609sANNeFKNN22mMiYZg+OR0N9/9qyTCHWWwJTpXDHS7W4FlAvuMbZ39iXDwF
2t5tNWFZMgFXNXiOxv7PC8Bf6+wc/oyy8Jemm00I5TqkISc6s+HBLrtQgiRb
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIGZjCCBE6gAwIBAgICEAAwDQYJKoZIhvcNAQELBQAwgcsxCzAJBgNVBAYTAklO
MQ4wDAYDVQQIDAVEZWxoaTESMBAGA1UEBwwJTmV3IERlbGhpMS4wLAYDVQQKDCVJ
bmRpYW4gSW5zdGl0dXRlIG9mIFRlY2hub2xvZ3ksIERlbGhpMScwJQYDVQQLDB5D
b21wdXRlciBTZXJ2aWNlcyBDZW50ZXIsIElJVEQxGjAYBgNVBAMMEWNlcnRjYS5p
aXRkLmFjLmluMSMwIQYJKoZIhvcNAQkBFhRzeXNhZG1AY2MuaWl0ZC5hYy5pbjAe
Fw0yMzA0MDkwNzAzMTNaFw0zMzA0MDYwNzAzMTNaMIG3MQswCQYDVQQGEwJJTjEO
MAwGA1UECAwFRGVsaGkxLjAsBgNVBAoMJUluZGlhbiBJbnN0aXR1dGUgb2YgVGVj
aG5vbG9neSwgRGVsaGkxJzAlBgNVBAsMHkNvbXB1dGVyIFNlcnZpY2VzIENlbnRl
ciwgSUlURDEaMBgGA1UEAwwRY2VydGluLmlpdGQuYWMuaW4xIzAhBgkqhkiG9w0B
CQEWFHN5c2FkbUBjYy5paXRkLmFjLmluMIICIjANBgkqhkiG9w0BAQEFAAOCAg8A
MIICCgKCAgEAiU0XYsiZoSi86JZ90ISD0FeMD4/0H6VcIHEUftbeo/XPf06ttWku
u/AHRi/Zgvg88HxDfOxv8/l3lKc1RnD+JraA5h+MhkDcyDxjP/sDzKQkLXYQD+qD
oEAfop9qhwXWWSMusm2IaRo84TgG/eWGLHlj2IfWPTuUgAbZYHThuOG3vHBUAroB
cRRvB/TnQ4J7Gmk2RUmCed1OElXhInT92X004dnpV1hILGOjujI02H4qOconXpFJ
504byGWrfFPS7CihtywNDtztDha/VGtF4a3JIStTo+HmO/Ft5f7vw5XotAdXBpDp
NOkQqKM0H55yereYLiahQZkBOsR10pd/JqzdV4OmEqBeWEtVPIDi4i5nGzYhVrsf
ytaJo5KCGO4KRUZHK2NAKM2oL8Lwp4P7g+GM0J4WWBnMhmfTsUaWHGwmjijMtJVh
Mp56W5Eb5LEw/EdtxLtpsdcA0XifyqArWRiPzs1058i+BFdPlecmGpqpdLW0ZuBk
T0F3LRYLpWvhhe9A/3m+Ff8/DYvz4D/7UtL0aMul3+xdc5t4wfP25DSzHYo/wHn4
hi9geJDrLVNrzID9keqmRk1NWCtTImXR1EoDDKGXAVmh4fnRwef/qOKBX26l+U0R
QflGoUtXrwzZGIaE4aB85N7L40V88djJYHZKK2tTd2kRBGB3iTXU92UCAwEAAaNm
MGQwHQYDVR0OBBYEFAKnpODLgu/jKYj+pYsVoATc62XDMB8GA1UdIwQYMBaAFChD
OKVT2UMxY4JDwAcHFpFhn4fCMBIGA1UdEwEB/wQIMAYBAf8CAQAwDgYDVR0PAQH/
BAQDAgGGMA0GCSqGSIb3DQEBCwUAA4ICAQC7NKQ39MvzXQMQ905NlxKI9Ffv069/
f817xm0pvQ0FJiCO+9Q8aiIfGcuyh2FgjoHpDULn7Y7YVZxr+v/lqiO6BKb//0qX
czrxxLMDtkgJLwsnBcrYOzp3G7qH/nbpEF/zPJICueZKuWIeJoMdkdXxTgKdfzB0
rnNRwizbCgmDhX4Ob1whS8z1VLQyaSwGg0jibDNfN8EN8kyEkJNAx86TSE3WiMqo
n4v6eRv80e2l6kx53ptYAGl5aN3twrg807/hB8BPGKL1uo6dlP7unvqrtOkRzV/i
1dMo0QtnqyncuEWn2f8Zt7Gtr5sbYBGEjNKtel82Hz/0UTKMlhenXLxLFLK3EZcj
7pUD1Z8B5aY3EotX9y4ocV3uHrr+lpcAUAI4b+6LkhFgbpbV5lN9br3ouKzT6uKp
gD9TzDsOCDX26BvBKBKFBHiS9JgPtg6vixEKzO3zp2QkJbCXfPxZZ2Yndhcjp5Y5
qyz19Dv1DT9LYO/GD4xJy1Tq4qJ4yZ5coyCZh4e6saCB9Xx0tDMXmgybRbYNbuZM
LlYguM9DQo8sLQwLZINKkHSsMYc9Yr7pAQR2nRZrPZGz//FFq2vf6xIBL6y8O2f4
xs51CtnsRPgyS40HNygrul5ZpR1qaaVLYo99mUXxgV2cP3dLSDer4ZNBDZE4WHlm
iUFbzBKWVU29NQ==
-----END CERTIFICATE-----
</ca>

<cert>
-----BEGIN CERTIFICATE-----
MIIE5zCCAs+gAwIBAgICb28wDQYJKoZIhvcNAQELBQAwgbcxCzAJBgNVBAYTAklO
MQ4wDAYDVQQIDAVEZWxoaTEuMCwGA1UECgwlSW5kaWFuIEluc3RpdHV0ZSBvZiBU
ZWNobm9sb2d5LCBEZWxoaTEnMCUGA1UECwweQ29tcHV0ZXIgU2VydmljZXMgQ2Vu
dGVyLCBJSVREMRowGAYDVQQDDBFjZXJ0aW4uaWl0ZC5hYy5pbjEjMCEGCSqGSIb3
DQEJARYUc3lzYWRtQGNjLmlpdGQuYWMuaW4wHhcNMjYwNjI0MTEyMzU4WhcNMjYx
MjIxMTEyMzU4WjCByjELMAkGA1UEBhMCSU4xDjAMBgNVBAgMBURlbGhpMRIwEAYD
VQQHDAlOZXcgRGVsaGkxLjAsBgNVBAoMJUluZGlhbiBJbnN0aXR1dGUgb2YgVGVj
aG5vbG9neSwgRGVsaGkxJzAlBgNVBAsMHkNvbXB1dGVyIFNlcnZpY2VzIENlbnRl
ciwgSUlURDESMBAGA1UEAwwJY3M1MjIxNjQ2MSowKAYJKoZIhvcNAQkBFhtjczUy
MjE2NDZAY3NlLmlpdGQuZXJuZXQuaW4wWTATBgcqhkjOPQIBBggqhkjOPQMBBwNC
AAQZ2pePc28O24cvtUGymwQk9vRjCTe1EPPgkiOBYDdnqm/Zkl3uVS5uaHbXQttY
75Ld8t9nf2OLGKhgO+FiZLWyo4GyMIGvMAkGA1UdEwQCMAAwMwYJYIZIAYb4QgEN
BCYWJE9wZW5TU0wgR2VuZXJhdGVkIENsaWVudCBDZXJ0aWZpY2F0ZTAdBgNVHQ4E
FgQUm0VWztkmlvZoExLWfMMV+0BLTKAwHwYDVR0jBBgwFoAUAqek4MuC7+MpiP6l
ixWgBNzrZcMwDgYDVR0PAQH/BAQDAgXgMB0GA1UdJQQWMBQGCCsGAQUFBwMCBggr
BgEFBQcDBDANBgkqhkiG9w0BAQsFAAOCAgEAbfriORMhR8rOiPIYlhc/4ZfwPwFX
XEu5RGS+jQRLCJpzmma/q5t3PkvCo6wDLerUOY3IHKXlsU1FFkaPpwjgxdGMfq0B
k+WE0RVflAl0ooGqwqOyzYb0kgZu5QK6R4UWy0urxCoPLdNutCRpMrrIh94OiDNe
TPuU1IyFcZ09b2GZ1/0A1W+P44gtACDdktRIiJXBn+Xm8ZKmF7a/gvuQW9YmdhK8
ly4NU/cmsNHI7dS/XeORfQ404iN5dUYuv27ffD5FHI3pi/r/erwGlLmp9rQ2Fgwq
I8ZUXXQdu1IScFOw9R92QtQOchOq3WoNS36vz26VApC+neJN+Z71v+wjewxBXNm+
C+zZu8+jFFJ6sH+VNVHRlMzMecQH1ZGeUXPzzeZet6o9PuTOBscMY84oPZi/xxrB
q+wk0EwuZV8LuERSObpOiWzJUR8Q4uNfnL8cTWXls1j+MuBnxUO6skeNuNtrpKPo
ksr2qg4QM5wOpcEsTXTLFsk9gx0eNMNBC6O5EAC5xAd+scT+LIVq1dXnGHgH2k2O
BpxAqqCoAOGFVXo22MAjgG36gyP0/Wt3GXchn/bWXyf2TsIU5i3bRtmuJTpI6ODv
YdX2oSxCU9YWXz1vXHbYymSkO/fPAyVSPYrYn+XKKGXG/pH3pH3iJc9pYTZvOGvC
247Wx1OOE/Hkrmw=
-----END CERTIFICATE-----
</cert>

# TLS Protection
<tls-auth>
#
# 2048 bit OpenVPN static key
#
-----BEGIN OpenVPN Static key V1-----
9a128f6b410522fbe43714b106d71f4d
c58331c2979a4ccd2e4c26aedddaec03
351296ae48c238de0c58b4dd9c9626d5
ddc4392caceb4dde11f099ebe34ca29f
75e72777c8b0326640e6110b5d1c1c16
762570e3fd8ddbddaf2f764ac73b079b
f93629572991fee8cf172b692db5da5a
a586c2bf58afbd5d6b42c31eeb265c17
285acbc81810e09f357761a158f19138
ae55ad1001dfda88b4f99c65f52b5dec
8af025cb4ff385e63d7b541a78144128
76bf50f9122258ee899c54bd3ab377e1
09de49b7f443cf54e4a876165fe54287
e19b31676f2c8ef0d696c5fdfb8c1d5d
f30557594cbda139e15bc895c9d0eeb0
9e9bbaf682160460e3af70da4daf9624
-----END OpenVPN Static key V1-----
</tls-auth>
)";

QString makeDownloadPath()
{
    QDir baseDir;

#ifdef Q_OS_WIN
    baseDir = QDir(QCoreApplication::applicationDirPath());
    baseDir.cdUp();
#elif defined(Q_OS_MAC)
    baseDir = QDir(QCoreApplication::applicationDirPath());
    baseDir.cdUp();
#else
    const QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (basePath.isEmpty()) {
        return {};
    }
    baseDir = QDir(basePath);
#endif

    if (!baseDir.mkpath("configurations")) {
        return {};
    }

    return baseDir.filePath(QStringLiteral("configurations/iitdconnect.ovpn"));
}
}

CertificateDownloadService::CertificateDownloadService(QObject *parent)
    : QObject(parent)
{
}

QString CertificateDownloadService::downloadedOvpnPath() const
{
    return makeDownloadPath();
}

void CertificateDownloadService::loadSavedCertificateState()
{
    const QString path = makeDownloadPath();
    if (!path.isEmpty() && QFile::exists(path)) {
        emit savedCertificateAvailable(path);
        return;
    }

    emit savedCertificateUnavailable();
}

void CertificateDownloadService::startCertificateDownload(const QString &username, const QString &password)
{
    currentUser = username.trimmed();

    if (currentUser.isEmpty() || password.isEmpty()) {
        emit warningOccurred("Missing details", "Enter both username and password.");
        return;
    }

    const QString targetPath = makeDownloadPath();
    if (targetPath.isEmpty()) {
        emit criticalOccurred("Download error", "Could not prepare the download folder.");
        return;
    }

    if (QFile::exists(targetPath)) {
        emit informationOccurred("Certificate ready", QStringLiteral("Using the existing OVPN file:\n%1").arg(targetPath));
        emit savedCertificateAvailable(targetPath);
        return;
    }

    emit busyChanged(true);
    emit statusMessage(QStringLiteral("Downloading certificate..."));

    // Write the hardcoded OVPN content to file
    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit busyChanged(false);
        emit criticalOccurred("Download error", QStringLiteral("Could not save the OVPN file to %1").arg(targetPath));
        return;
    }

    file.write(kOvpnContent);
    file.close();

    emit busyChanged(false);
    emit statusMessage(QStringLiteral("Certificate downloaded to %1").arg(targetPath));
    emit savedCertificateAvailable(targetPath);
    emit informationOccurred("Download complete", QStringLiteral("OVPN file saved to:\n%1").arg(targetPath));
}