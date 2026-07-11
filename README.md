# IIT Delhi VPN Client

A cross-platform Qt6/C++ GUI application for connecting to the IIT Delhi VPN service. Supports **Linux**, **Windows**, and **macOS** with platform-specific keystore and VPN backend implementations.

---

## Table of Contents

1. [How to Run the Workflows](#1-how-to-run-the-workflows)
2. [Keystore Implementation](#2-keystore-implementation)
3. [VPN Backend Implementation](#3-vpn-backend-implementation)
4. [Certificate Lifecycle & Parsing](#4-certificate-lifecycle--parsing)
5. [Displaying Logs to the User](#5-displaying-logs-to-the-user)
6. [Connection Details, Speeds & Graph](#6-connection-details-speeds--graph)

---

## 1. GitHub CI/CD Workflows

All workflows are triggered manually via `workflow_dispatch` (from the GitHub Actions tab). There is no automatic trigger on push or pull request.

### Linux Build (`.github/workflows/lin-build.yml`)

**How to run:** Go to the GitHub repository → **Actions** → **Linux IITDelhiVPN GUI** → **Run workflow**.

**What it does:** Builds the application for three Linux targets in parallel using a build matrix:

| Target | Runner | Qt Arch |
|---|---|---|
| Ubuntu 22.04 amd64 | `ubuntu-22.04` | `linux_gcc_64` |
| Ubuntu 24.04 amd64 | `ubuntu-24.04` | `linux_gcc_64` |
| Ubuntu 24.04 arm64 | `ubuntu-24.04-arm` | `linux_gcc_arm64` |

**Steps:**
1. Checks out the repository
2. Installs Qt 6.10.1 via `jurplel/install-qt-action`
3. Configures with CMake (GCC, Release)
4. Builds the `IITDelhiVPN` binary
5. Runs **LinuxDeploy** to create an AppDir with bundled Qt plugins (`libqxcb`, `libqwayland`) and the desktop file / icon
6. Filters out system libraries, translations, and documentation from the AppDir
7. Packages the AppDir into a tarball

**Output produced:** A tarball artifact named `iitdvpn-gui-lin-<runner>.tar` containing the `usr/` AppDir with the `IITDelhiVPN` binary and all bundled dependencies.

---

### Windows Build (`.github/workflows/win-build.yml`)

**How to run:** Go to the GitHub repository → **Actions** → **Windows IITDelhiVPN GUI** → **Run workflow**.

**What it does:** Builds the application for two Windows targets in parallel:

| Target | Runner | MSVC Arch | Qt Arch | CMake Arch |
|---|---|---|---|---|
| Windows 11 amd64 | `windows-2022` | `amd64` | `win64_msvc2022_64` | `x64` |
| Windows 11 arm64 | `windows-2022` | `amd64_arm64` | `win64_msvc2022_arm64_cross_compiled` | `ARM64` |

**Steps:**
1. Checks out the repository
2. Sets up the MSVC developer command prompt via `ilammy/msvc-dev-cmd`
3. Installs Qt (6.10.1 for amd64, 6.9.1 for arm64) via `jurplel/install-qt-action`
4. Configures with CMake (MSVC `cl`, Release)
5. Builds `IITDelhiVPN.exe`
6. Runs `windeployqt` to bundle required Qt DLLs
7. Cleans up unnecessary files (OpenGL software renderer, D3D compiler, unused image format plugins, VC redistributables)
8. Uploads the `Release/` directory as a build artifact

**Output produced:** A build artifact named `iitdvpn-gui-win-<arch>` containing `IITDelhiVPN.exe` with all required Qt DLLs and plugins.

---

### macOS Build (`.github/workflows/mac-build.yml`)

**How to run:** Go to the GitHub repository → **Actions** → **Mac IITDelhiVPN GUI** → **Run workflow**.

**What it does:** Builds the application for four macOS targets in parallel:

| Target | Runner | Qt Arch |
|---|---|---|
| macOS arm64 (14) | `macos-14` | `clang_64` |
| macOS arm64 (15) | `macos-15` | `clang_64` |
| macOS arm64 (26) | `macos-26` | `clang_64` |
| macOS amd64 (15) | `macos-15-intel` | `clang_64` |

**Steps:**
1. Checks out the repository
2. Installs Qt 6.10.1 via `jurplel/install-qt-action`
3. Configures with CMake (Clang, Release)
4. Builds `IITDelhiVPN.app`
5. Runs `macdeployqt` to bundle Qt frameworks into the `.app` bundle
6. Packages the `.app` into a tarball
7. Uploads the tarball as a build artifact

**Output produced:** A tarball artifact named `OpenVPN3-GUI-<target-name>.tar` containing the `IITDelhiVPN.app` bundle with all Qt frameworks deployed inside.

---

## 2. Keystore Implementation

### Architecture

All keystore implementations derive from the abstract `IKeyStore` interface (`headers/ikeystore.h`):

```cpp
class IKeyStore : public QObject {
    virtual bool checkKeyExists() = 0;
    virtual void generateKey() = 0;
    virtual QByteArray signData(const QByteArray &data) = 0;
    virtual QByteArray generateCsr(const QString &username, const QString &email) = 0;
    virtual void clearKey() = 0;
};
```

The key name constant is `TestVPNKey` (used on Windows for the CNG key container).

### LinuxKeyStore (`src/linuxkeystore.cpp`)

| Operation | Implementation |
|---|---|
| **Storage** | PEM file at `~/.config/IITDelhiVPN/pkey.pem` with `0600` permissions |
| **Key generation** | `openssl ecparam -name prime256v1 -genkey -noout` → ECDSA P-256 |
| **Key existence check** | Reads the PEM file and validates it with `openssl ec` |
| **Signing** | Writes key to a temporary file, runs `openssl pkeyutl -sign -inkey <tmp> -pkeyopt digest:sha256` |
| **CSR generation** | Writes key to a temporary file, runs `openssl req -new -key <tmp> -subj <subject>` with subject `/C=IN/ST=Delhi/L=New Delhi/O=Indian Institute of Technology, Delhi/OU=Computer Services Center, IITD/CN=<username>/emailAddress=<email>` |
| **Key deletion** | `QFile::remove()` on the PEM file |

**Security note:** The private key is stored on the filesystem with user-only read/write permissions. The key is written to a temporary file during signing/CSR operations, which is immediately removed after use.

### WindowsKeyStore (`src/windowskeystore.cpp`)

| Operation | Implementation |
|---|---|
| **Storage** | TPM-backed via `Microsoft Platform Crypto Provider` CNG key container |
| **Key generation** | PowerShell: `[System.Security.Cryptography.CngKey]::Create(ECDsaP256, "TestVPNKey", ...)` with `OverwriteExistingKey` |
| **Key existence check** | PowerShell: `[System.Security.Cryptography.CngKey]::Open("TestVPNKey", <provider>)` wrapped in try/catch |
| **Signing** | PowerShell: `ECDsaCng.SignData(data, SHA256)` → returns P1363 signature → converted to DER via Windows `CryptEncodeObjectEx` with `X509_ECC_SIGNATURE` |
| **CSR generation** | Writes an INF file → runs `certreq.exe -new <inf> <csr>` with `UseExistingKeySet = True` and `ProviderName = "Microsoft Platform Crypto Provider"` |
| **Key deletion** | PowerShell: `CngKey.Delete()` |

**P1363 → DER conversion:** Windows CNG returns ECDSA signatures in P1363 format (r||s, 64 bytes). The OpenVPN management interface expects DER-encoded signatures. The conversion uses the Windows CryptoAPI `CryptEncodeObjectEx` with `X509_ECC_SIGNATURE` after byte-reversing r and s components (little-endian to big-endian).

### MacKeyStore (`src/mackeystore.cpp`)

**⚠️ Stub implementation.** All methods log a warning and return default values. Not yet functional.

---

## 3. VPN Backend Implementation

### Architecture

Both backends implement the `IVpnBackend` interface (`headers/ivpnbackend.h`):

```cpp
class IVpnBackend : public QObject {
    virtual void connectVpn(const QString &ovpnPath, const QString &password) = 0;
    virtual void disconnectVpn() = 0;
    virtual VpnConnectionState connectionState() const = 0;

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &message);
    void logLineReceived(const QString &line);
    void connectionStateChanged(VpnConnectionState state);
    void stateChanged(const QString &state);
    void byteCountChanged(qulonglong uploadBytes, qulonglong downloadBytes);
    void connectionInfoChanged(const QString &remote, const QString &remoteAddr,
                               const QString &proto, const QString &localIface,
                               const QString &localIp, const QString &gateway, int mtu);
    void connectionStepChanged(int stepIndex);
};
```

### LinuxVpnBackend (`src/linuxvpnbackend.cpp`)

Uses the `iitdvpn` CLI (the openvpn3-linux management tool).

#### Connection Steps

| Index | State | Label |
|---|---|---|
| 0 | `CONNECTING` | Connecting |
| 1 | `CONNECTED` | Connected |

#### Connection Flow

1. **Config import:** `iitdvpn config-import --config <ovpn> --name <uuid>` — imports the OVPN configuration. If this fails, emits `errorOccurred("The configuration file seems to be corrupted.")`.
2. **Log monitoring:** `iitdvpn log --config <uuid>` — starts a QProcess that tails the OpenVPN log.
3. **Session start:** `iitdvpn session-start --config <uuid> --connection-timeout 10` — starts the VPN session. The password is written to the process's stdin.
4. **Log parsing** (`onLogOutput`): Reads log lines and detects:
   - `[STATUS] Connection, Client connecting` → step 0
   - `[STATUS] Connection, Client connected` → step 1, starts stats timer
   - `[STATUS] Connection, Client authentication failed` → friendly auth error
   - `[STATUS] Connection, Client connection failed` → generic error
   - `[STATUS] Connection, Client disconnecting: Connection timeout` → timeout error with troubleshooting tips
   - `Client INFO: Connected:` → parses connection info (remote, protocol, interface, IP, gateway, MTU)

#### Stats Polling

A `QTimer` fires every 1 second to run `iitdvpn session-stats --config <uuid>`. The output is parsed for `BYTES_IN` and `BYTES_OUT` fields, and `byteCountChanged` is emitted.

#### Disconnection Flow

1. `iitdvpn session-manage --config <uuid> --disconnect`
2. Stops stats timer, kills log process
3. `iitdvpn config-remove --config <uuid>` (with confirmation "YES" written to stdin)
4. Cleans up state

#### Error Handling

| Condition | User-Facing Message |
|---|---|
| Config import fails | "The configuration file seems to be corrupted. Please download the configuration file again." |
| Auth failure | "The password that you entered is incorrect. Repeatedly entering an incorrect password may result in a temporary block. Please try again." |
| Connection timeout | "The server could not be reached or it failed to respond. Common causes are: i. You are already on IITD network. Please connect externally. ii. You are behind a firewall blocking port 1194." |
| Generic failure | "Connection failed" |

### WinMacVpnBackend (`src/winmacvpnbackend.cpp`)

Uses the OpenVPN community client binary (`iitdvpncli` / `iitdvpncli.exe`) with the **management interface** over TCP.

#### Connection Steps

| Index | State | Label |
|---|---|---|
| 0 | `RESOLVE` | Resolving server address |
| 1 | `WAIT` | Waiting for server response |
| 2 | `CONNECTING` | Connecting to server |
| 3 | `GET_CONFIG` | Downloading configuration options |
| 4 | `ASSIGN_IP` | Assigning IP address |

#### Connection Flow

1. **Process start:** Launches `iitdvpncli` with arguments:
   ```
   --config <ovpn> --connection-timeout 10
   --management 127.0.0.1 7505
   --management-query-passwords
   --auth-use-cert-cn-username
   --management-external-key
   ```
2. **Management socket:** When the process outputs "OMI Listening", the backend connects a `QTcpSocket` to `127.0.0.1:7505`.
3. **Password handshake:** When the management interface sends `>PASSWORD:Need 'Auth' password`, the backend responds:
   - `log on\n` — enables log output
   - `state on\n` — enables state change notifications
   - `bytecount 1\n` — enables 1-second byte count updates
   - `password Auth <password>\n` — sends the VPN password

#### Management Protocol Handling (`handleMgmtLine`)

| Tag | Handling |
|---|---|
| `>PASSWORD:Need 'Auth' password` | Enables log/state/bytecount, sends password |
| `>FATAL:...` | Parses the fatal error and emits a friendly message |
| `>STATE:...` | Updates connection step index, detects CONNECTED/EXITING/RECONNECTING |
| `>LOG:...` | Formats with timestamp, emits as log line, parses CONNECTED info |
| `>BYTECOUNT:<bytes_in>,<bytes_out>` | Emits `byteCountChanged` |
| `>RSA_SIGN:<base64_data>` | Delegates signing to the keystore via `QtConcurrent::run` |

#### External PKI (RSA_SIGN)

The `--management-external-key` flag tells OpenVPN to request signing via the management interface. When `>RSA_SIGN:<base64>` is received:

1. The base64 payload is decoded
2. `m_keyStore->signData(rawData)` is called on a background thread via `QtConcurrent::run`
3. When the signature is ready (`onSignDataFinished`), it's sent back:
   ```
   rsa-sig
   <base64_signature>
   END
   ```

#### Error Handling

| Condition | User-Facing Message |
|---|---|
| `auth-failure` | "The password that you entered is incorrect..." |
| `CONNECTION_TIMEOUT` during RESOLVE | "The DNS resolution could not be done..." |
| `CONNECTION_TIMEOUT` during WAIT | "The server could not be reached or it failed to respond..." |
| Generic `CONNECTION_TIMEOUT` | "The connection timed out while contacting the server..." |
| `eval config error` | "The .ovpn file is malformed. Please download the file again." |
| `creds error` | "The dynamic challenge cookie from the server was malformed." |

#### Disconnection Flow

Sends `signal SIGTERM\n` over the management socket. The OpenVPN process exits cleanly.

---

## 4. Certificate Lifecycle & Parsing

### CertificateDownloadService (`src/certificatedownloadservice.cpp`)

Manages the full lifecycle from key generation to signed certificate download.

#### Lifecycle Steps

```
User clicks "Generate"
        │
        ▼
┌─────────────────┐
│  Authenticate   │  (simulated — currently hardcodes email)
│  User           │
└────────┬────────┘
         │  emit authenticationComplete(email)
         ▼
┌─────────────────┐
│  Generate CSR   │  keystore->generateCsr(username, email)
│  via Keystore   │
└────────┬────────┘
         │  emit csrGenerated(csrData)
         ▼
┌─────────────────┐
│  Submit CSR     │  (simulated — writes pre-baked OVPN content)
│  to Server      │
└────────┬────────┘
         │  emit savedCertificateAvailable(path)
         ▼
┌─────────────────┐
│  OVPN File      │  Written to:
│  Ready          │  Linux:   ~/.config/IITDelhiVPN/configurations/iitdconnect.ovpn
│                 │  Windows: %LOCALAPPDATA%/IITDelhiVPN/configurations/iitdconnect.ovpn
│                 │  macOS:   <app bundle>/configurations/iitdconnect.ovpn
└─────────────────┘
```

**Note:** The CSR submission step is currently simulated — the service writes a pre-baked OVPN template (`kOvpnContent`) that includes the CA certificate, TLS auth key, and a placeholder client certificate. The actual server-side signing pipeline is not yet integrated.

#### Google Time Fetch

After a certificate is parsed, the service fetches the current time from Google's `Date` HTTP header (`HEAD https://www.google.com`). This is used to accurately determine certificate validity status regardless of the system clock. If the fetch fails, it retries every 5 seconds and falls back to `QDateTime::currentDateTimeUtc()`.

### CertificateBoxWidget (`src/certificateboxwidget.cpp`)

Displays certificate information parsed from the downloaded `.ovpn` file.

#### Certificate Parsing (`loadFromOvpnFile`)

1. Opens the `.ovpn` file
2. Extracts the PEM certificate block between `<cert>` and `</cert>` tags using regex
3. Parses the PEM with `QSslCertificate::fromData(pemData, QSsl::Pem)`
4. Extracts subject fields:
   - **CommonName (CN)** — typically the username
   - **Organization (O)** — "Indian Institute of Technology, Delhi"
   - **EmailAddress** — user's institute email
5. Extracts validity dates: `effectiveDate()` and `expiryDate()`
6. Emits `certificateParsed()` to trigger Google time fetch

#### Validity Display (`updateValidityDisplay`)

| Condition | Status Pill |
|---|---|
| `now > expiryDate` | **Expired** (red) — shows "Generate New" button |
| `now < effectiveDate` | **Not Yet Valid** (amber) |
| `effectiveDate <= now <= expiryDate` | **Active** (green) |

The `ValidityTimelineWidget` provides a visual timeline showing the certificate's validity period relative to the current time.

#### UI States

- **No certificate:** Shows a "Generate" button and a progress widget during download
- **Certificate present:** Shows user info, validity timeline, and status pill with "Renew" option

---

## 5. Displaying Logs to the User

### Log Collection

Logs are collected from the VPN backend via the `logLineReceived` signal:

```cpp
connect(backend.get(), &IVpnBackend::logLineReceived,
        this, [this](const QString &line) {
            recentConnectionLogs.push_back(line);
        });
```

The `recentConnectionLogs` QStringList accumulates logs from the most recent connection session. It is cleared at the start of each new connection attempt.

### Log Sources

| Platform | Source | Format |
|---|---|---|
| **Linux** | `iitdvpn log` stdout | Raw OpenVPN log lines from the openvpn3-linux session |
| **Windows/macOS** | OpenVPN management interface `>LOG:` messages | Formatted with timestamp: `[yyyy-MM-dd HH:mm:ss] <message>` |

### ConnectionLogsDialog (`src/connectionlogsdialog.cpp`)

A modal dialog that displays logs to the user:

- **QPlainTextEdit** — read-only, monospace font (Courier New, 10pt), word wrap enabled
- **Placeholder text** — "Logs from the most recent VPN socket connection will appear here."
- **Save button** — exports logs to a timestamped `.txt` file (`vpn-logs-<yyyyMMdd-HHmmss>.txt`) in the user's Documents folder
- **Close button** — dismisses the dialog

The dialog is opened from the main window's "Logs" button (`on_logsButton_clicked`).

---

## 6. Connection Details, Speeds & Graph

### Connection Info Display

When the VPN connects, the backend emits `connectionInfoChanged` with parsed connection details. The main window (`handleVpnConnectionInfoChanged`) parses the `remote` string to extract:

| Field | Source | Display Label |
|---|---|---|
| **Client Name** | Username from `user@host` in remote string | `clientNameLabel` |
| **Client IP** | Local IP assigned by VPN | `clientDetailsCol1Label` |
| **Tunnel Interface** | e.g., `tun0` | `clientDetailsCol2Label` |
| **Server Name** | Hostname from remote string | `serverNameLabel` |
| **Server IP** | IP address in parentheses | `serverDetailsCol1Label` |
| **Server Port** | Port after colon | `serverDetailsCol2Label` |
| **Protocol** | e.g., `UDP` | `serverDetailsCol3Label` |

### Traffic Statistics

#### Data Source

| Platform | Mechanism | Interval |
|---|---|---|
| **Linux** | `iitdvpn session-stats --config <uuid>` (BYTES_IN / BYTES_OUT) | 1 second (QTimer) |
| **Windows/macOS** | OpenVPN management `bytecount 1` command → `>BYTECOUNT:<in>,<out>` | 1 second (OpenVPN built-in) |

#### Speed Calculation (`TrafficGraphWidget::onByteCountReceived`)

1. On first call, records baseline byte counts and timestamp
2. On subsequent calls:
   - `elapsedSeconds = (now - lastTimestamp) / 1000.0`
   - `uploadSpeed = (currentUploadBytes - lastUploadBytes) / elapsedSeconds`
   - `downloadSpeed = (currentDownloadBytes - lastDownloadBytes) / elapsedSeconds`
3. Appends a sample point with timestamp, upload speed, and download speed

### TrafficGraphWidget (`src/trafficgraphwidget.cpp`)

A custom-painted `QWidget` that renders a real-time speed graph.

#### Features

- **Dual series:** Upload (red, `#C21717`) and download (blue, `#1E88E5`)
- **Time window:** 60 seconds of history
- **Max samples:** 180 data points
- **Grid lines:** 10 horizontal grid lines
- **Speed labels:** Current upload and download speeds displayed at the top
- **Max speed indicator:** Peak speed shown at top-right
- **Fill under curve:** Semi-transparent fill under each series
- **Adaptive formatting:** Speeds displayed in B/s, KB/s, MB/s, or GB/s with appropriate precision

#### Paint Cycle

1. Draws background and grid lines
2. For each series (upload/download):
   - Builds a `QPainterPath` through all sample points
   - Creates a fill path down to the bottom of the plot area
   - Fills with semi-transparent color (alpha=28)
   - Draws the line with 0.5px pen width
3. Draws speed labels at the top of the plot area

#### Reset

`resetTraffic()` clears all samples, invalidates the timer, and resets byte counters. Called when the VPN disconnects or a new connection starts.

### UI Layout

The traffic graph and connection details are displayed on the **disconnect page** (shown when connected):

- **Connection details section:** Client info (name, IP, tunnel) and server info (name, IP, port, protocol)
- **Traffic stats section:** Upload/download speed values, traffic graph widget
- **Disconnect button:** Ends the VPN session

The traffic stats frame and speed labels are hidden until the connection is established.