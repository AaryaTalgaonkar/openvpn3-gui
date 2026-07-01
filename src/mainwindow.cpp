#include "mainwindow.h"
#include "thememanager.h"
#include "systemtraymanager.h"

#include <QDir>
#include <QFile>
#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QPainter>
#include <QFile>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QRegularExpression>
#include <QPlainTextEdit>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QUrlQuery>
#include <QElapsedTimer>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QNetworkAccessManager>

#ifdef Q_OS_LINUX
#include "linuxvpnbackend.h"
#include "linuxkeystore.h"
#elif defined(Q_OS_WIN)
#include "winmacvpnbackend.h"
#include "windowskeystore.h"
#elif defined(Q_OS_MAC)
#include "winmacvpnbackend.h"
#include "mackeystore.h"
#endif

namespace {

#ifdef Q_OS_LINUX
// Linux backend does not yet define connection steps; use a local minimal set.
const ConnectionStepDefinition kConnectionSteps[] = {
    {"CONNECTING", "↻", "Connecting to server"},
};
constexpr int kConnectionStepCount = static_cast<int>(sizeof(kConnectionSteps) / sizeof(kConnectionSteps[0]));
#elif defined(Q_OS_WIN) || defined(Q_OS_MAC)
const ConnectionStepDefinition* const kConnectionSteps = WinMacVpnBackend::kWinMacConnectionSteps;
const int kConnectionStepCount = WinMacVpnBackend::kWinMacConnectionStepCount;
#endif

constexpr int kVpnPasswordDialogWidth = 250;

void setPointingHandCursorForButtons(QWidget *widget)
{
    for (QPushButton *button : widget->findChildren<QPushButton *>()) {
        if (button) {
            button->setCursor(Qt::PointingHandCursor);
        }
    }
}

bool getCredentialsFromDialog(QWidget *parent, const QString &title, const QString &promptText, QString *username, QString *password)
{
    if (!username || !password) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setFixedWidth(300);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 20, 20, 20);

    if (!promptText.isEmpty()) {
        auto *infoLabel = new QLabel(promptText, &dialog);
        infoLabel->setWordWrap(true);
        layout->addWidget(infoLabel);
    }

    auto *userEdit = new QLineEdit(&dialog);
    userEdit->setPlaceholderText("Username");
    layout->addWidget(userEdit);

    auto *passEdit = new QLineEdit(&dialog);
    passEdit->setPlaceholderText("Password");
    passEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(passEdit);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QTimer::singleShot(0, &dialog, [&dialog]() {
        setPointingHandCursorForButtons(&dialog);
    });
    dialog.adjustSize();

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    *username = userEdit->text().trimmed();
    *password = passEdit->text();
    return !username->isEmpty() && !password->isEmpty();
}

bool getPasswordFromDialog(QWidget *parent, const QString &title, const QString &promptText, QString *password)
{
    if (!password) {
        return false;
    }

    QInputDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setLabelText(promptText);
    dialog.setInputMode(QInputDialog::TextInput);
    dialog.setTextEchoMode(QLineEdit::Password);
    dialog.setSizeGripEnabled(false);
    if (QLabel *label = dialog.findChild<QLabel *>()) {
        label->setWordWrap(true);
        label->setMinimumWidth(kVpnPasswordDialogWidth);
    }
    QTimer::singleShot(0, &dialog, [&dialog]() {
        setPointingHandCursorForButtons(&dialog);
    });
    dialog.setFixedWidth(kVpnPasswordDialogWidth);
    dialog.adjustSize();
    dialog.setFixedWidth(kVpnPasswordDialogWidth);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    *password = dialog.textValue();
    return !password->isEmpty();
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , settings("IIT Delhi", "IITDelhiVPN")
{
    ui->setupUi(this);
    connectUi.setupUi(ui->connectPage);
    connectingUi.setupUi(ui->connectingPage);
    disconnectUi.setupUi(ui->disconnectPage);
    getStartedUi.setupUi(ui->getStartedPage);
    ui->rootStack->setCurrentWidget(ui->normalContentPage);

#ifdef Q_OS_LINUX
    backend = std::make_unique<LinuxVpnBackend>(this);
#elif defined(Q_OS_WIN) || defined(Q_OS_MAC)
    backend = std::make_unique<WinMacVpnBackend>(this);
#endif

    // Create platform-specific keystore
#ifdef Q_OS_LINUX
    keystore = std::make_unique<LinuxKeyStore>(this);
#elif defined(Q_OS_WIN)
    keystore = std::make_unique<WindowsKeyStore>(this);
#elif defined(Q_OS_MAC)
    keystore = std::make_unique<MacKeyStore>(this);
#endif

    // Pass the keystore to the certificate download service for CSR generation
    certificateService.setKeyStore(keystore.get());

    // Pass the keystore to the backend for RSA_SIGN operations
    if (backend) {
        auto *winMacBackend = qobject_cast<WinMacVpnBackend *>(backend.get());
        if (winMacBackend) {
            winMacBackend->setKeyStore(keystore.get());
        }
    }

    // Create and set up the connection progress widget (self-contained)
    {
        auto *progressWidget = new ConnectionProgressWidget(connectingUi.progressRingHost);
        progressWidget->setupSteps(connectingUi.stepsLayout, connectingUi.progressRingHostLayout,
                                   kConnectionSteps, kConnectionStepCount);
    }

    // Create the certificate box widget and embed it into the placeholder container
    certBox = new CertificateBoxWidget(connectUi.certInfoContainer);
    {
        auto *containerLayout = new QVBoxLayout(connectUi.certInfoContainer);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->addWidget(certBox);
    }

    // Create a single traffic graph widget and add it to the disconnect screen.
    trafficGraphWidget = new TrafficGraphWidget(nullptr);
    trafficGraphWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    disconnectUi.trafficStatsLayout->addWidget(trafficGraphWidget);

    disconnectUi.connectedTrafficFrame->setVisible(false);
    if (disconnectUi.uploadStatCard) disconnectUi.uploadStatCard->setVisible(false);
    if (disconnectUi.downloadStatCard) disconnectUi.downloadStatCard->setVisible(false);
    disconnectUi.uploadSpeedValue->setText(QStringLiteral("--"));
    disconnectUi.downloadSpeedValue->setText(QStringLiteral("--"));

    darkTheme = settings.value("theme/dark", false).toBool();
    ThemeManager::applyTheme(darkTheme, ui->themeToggleButton);

    connectUi.connectButton->setCheckable(false);
    connectUi.connectButton->setText(QStringLiteral("⏻")); // Set to power-on emoji
    connect(certBox, &CertificateBoxWidget::generateClicked,
            this, &MainWindow::handleDownloadButtonClicked);
    connect(getStartedUi.getStartedButton, &QPushButton::clicked,
            this, &MainWindow::handleGetStartedClicked);
    connect(connectUi.connectButton, &QPushButton::clicked,
            this, &MainWindow::handleConnectButtonClicked);
    connect(disconnectUi.disconnectButton, &QPushButton::clicked,
            this, &MainWindow::handleConnectButtonClicked);

    connect(backend.get(), &IVpnBackend::stateChanged,
            this, &MainWindow::handleVpnStateChanged);

    connect(backend.get(), &IVpnBackend::byteCountChanged,
            this, &MainWindow::handleVpnByteCountChanged);

    connect(backend.get(), &IVpnBackend::connectionInfoChanged,
            this, &MainWindow::handleVpnConnectionInfoChanged);

    connect(backend.get(), &IVpnBackend::connectionStateChanged,
            this, &MainWindow::handleVpnConnectionStateChanged);

    connect(backend.get(), &IVpnBackend::errorOccurred,
            this, &MainWindow::handleVpnError);

    connect(backend.get(), &IVpnBackend::logLineReceived,
            this, [this](const QString &line) {
            recentConnectionLogs.push_back(line);
            });

    connect(&certificateService, &CertificateDownloadService::savedCertificateAvailable,
            this, [this](const QString &) {
                setConnectFlow();
            });

    connect(&certificateService, &CertificateDownloadService::savedCertificateUnavailable,
            this, [this]() {
                setInitialFlow();
            });

    connect(&certificateService, &CertificateDownloadService::busyChanged,
            this, [this](bool busy) {
                certBox->setGenerateEnabled(!busy);
                certBox->showDownloadProgress(busy);
                if (!busy) {
                    certBox->setDownloadProgress(0);
                }
            });

    connect(&certificateService, &CertificateDownloadService::downloadProgressChanged,
            this, [this](int percent) {
                certBox->setDownloadProgress(percent);
            });

    connect(&certificateService, &CertificateDownloadService::statusMessage,
            this, [this](const QString &message) {
                ui->statusbar->showMessage(message, 5000);
                certBox->setDownloadStatus(message);
            });

    connect(&certificateService, &CertificateDownloadService::warningOccurred,
            this, [this](const QString &title, const QString &message) {
                QMessageBox::warning(this, title, message);
            });

    connect(&certificateService, &CertificateDownloadService::criticalOccurred,
            this, [this](const QString &title, const QString &message) {
                QMessageBox::critical(this, title, message);
            });

    connect(&certificateService, &CertificateDownloadService::informationOccurred,
            this, [this](const QString &title, const QString &message) {
                QMessageBox::information(this, title, message);
            });

    // When the certificate box parses an OVPN file, fetch accurate Google time
    connect(certBox, &CertificateBoxWidget::certificateParsed,
            this, [this]() {
                certificateService.fetchGoogleTime();
            });

    // Connect Google time sync signals from CertificateDownloadService
    connect(&certificateService, &CertificateDownloadService::googleTimeReceived,
            this, [this](const QDateTime &time) {
                certBox->updateValidityDisplay(time);
            });
    connect(&certificateService, &CertificateDownloadService::googleTimeFetchFailed,
            this, [this]() {
                certBox->updateValidityDisplay(QDateTime::currentDateTimeUtc());
            });

    // Check if the platform key exists to decide which screen to show
    if (keystore && keystore->checkKeyExists()) {
        // Key exists — proceed with existing certificate check flow
        loadSavedCertificateState();
    } else {
        // Key does NOT exist — show the Get Started page (full page, no top bar)
        ui->rootStack->setCurrentWidget(ui->getStartedPage);
    }

    // Set up system tray manager
    trayManager = std::make_unique<SystemTrayManager>(this, this);
    connect(trayManager.get(), &SystemTrayManager::showHideRequested,
            this, [this]() {
                if (isVisible()) {
                    hide();
                    trayManager->setShowHideActionText(QStringLiteral("Show"));
                } else {
                    show();
                    raise();
                    activateWindow();
                    trayManager->setShowHideActionText(QStringLiteral("Hide"));
                }
            });
    connect(trayManager.get(), &SystemTrayManager::quitRequested,
            this, [this]() {
                trayManager->setQuitting(true);
                if (backend->connectionState() == VpnConnectionState::Connected) {
                    backend->disconnectVpn();
                }
                QApplication::quit();
            });
    trayManager->setup();
}

void MainWindow::handleGetStartedClicked()
{
    if (!keystore) {
        return;
    }

    // Generate the platform key
    keystore->generateKey();

    // After generating the key, check for saved certificate
    loadSavedCertificateState();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setInitialFlow()
{
    ui->rootStack->setCurrentWidget(ui->normalContentPage);
    ui->screenStack->setCurrentWidget(ui->connectPage);
    certBox->showNoCertMode();
    connectUi.connectButton->setText(QStringLiteral("⏻")); // Set to power-on emoji
    disconnectUi.disconnectButton->setText(QStringLiteral("⏻"));
    disconnectUi.connectedTrafficFrame->setVisible(false);
    trafficGraphWidget->resetTraffic();
}

void MainWindow::setConnectFlow()
{
    ui->rootStack->setCurrentWidget(ui->normalContentPage);
    ui->screenStack->setCurrentWidget(ui->connectPage);
    updateCertificateInfoBox();
    showConnectPage();
}

void MainWindow::loadSavedCertificateState()
{
    certificateService.loadSavedCertificateState();
}

void MainWindow::showConnectPage()
{
    if (backend->connectionState() == VpnConnectionState::Connected) {
        ui->screenStack->setCurrentWidget(ui->disconnectPage);
        disconnectUi.connectionStatusTitle->setText(QStringLiteral("You are connected"));
        disconnectUi.connectionStatusSubtitle->setText(QStringLiteral("Live throughput is shown below"));
        disconnectUi.connectedTrafficFrame->setVisible(true);
        disconnectUi.disconnectButton->setText(QStringLiteral("⏻"));
    } else {
        ui->screenStack->setCurrentWidget(ui->connectPage);
        connectUi.connectionStatusTitle->setText(QStringLiteral("You are not connected"));
        connectUi.connectionStatusSubtitle->setText(QStringLiteral("Connect to access internal resources of IIT Delhi"));
        connectUi.connectButton->setText(QStringLiteral("⏻"));
        trafficGraphWidget->resetTraffic();
    }
    connectUi.connectButton->setEnabled(true);
    disconnectUi.disconnectButton->setEnabled(true);
    ui->statusbar->showMessage(backend->connectionState() == VpnConnectionState::Connected ? QStringLiteral("Status: Connected") : QStringLiteral("Status: Disconnected"));
}

void MainWindow::showConnectingPage()
{
    ui->screenStack->setCurrentWidget(ui->connectingPage);
    ui->statusbar->showMessage(QStringLiteral("Status: Connecting"));
}

void MainWindow::updateCertificateInfoBox()
{
    const QString ovpnPath = certificateService.downloadedOvpnPath();
    if (ovpnPath.isEmpty()) {
        certBox->showNoCertMode();
        return;
    }

    // Delegate OVPN parsing to the certificate box widget.
    // On success it will emit certificateParsed(), which triggers
    // the Google time fetch connected in the constructor.
    certBox->loadFromOvpnFile(ovpnPath);
}

void MainWindow::handleVpnStateChanged(const QString &state)
{
    // Find the progress widget in the connecting page
    auto *progressWidget = connectingUi.progressRingHost->findChild<ConnectionProgressWidget *>();
    if (progressWidget) {
        const int index = progressWidget->stepIndexForState(state);
        if (index >= 0) {
            progressWidget->updateStep(index);
            if (ui->screenStack->currentWidget() != ui->connectingPage) {
                showConnectingPage();
            }
        }
    }
}

void MainWindow::handleVpnConnectionStateChanged(VpnConnectionState state)
{
    auto *progressWidget = connectingUi.progressRingHost->findChild<ConnectionProgressWidget *>();

    switch (state) {
    case VpnConnectionState::Disconnected:
        if (progressWidget) {
            progressWidget->resetIndicators();
        }
        trafficGraphWidget->resetTraffic();
        showConnectPage();
        break;

    case VpnConnectionState::Connecting:
        showConnectingPage();
        break;

    case VpnConnectionState::Connected:
        if (progressWidget) {
            progressWidget->setProgressStep(kConnectionStepCount - 1, kConnectionStepCount);
            progressWidget->updateStep(kConnectionStepCount - 1);
        }
        trafficGraphWidget->resetTraffic();
        ui->statusbar->showMessage(QStringLiteral("Status: Connected"));
        // Move traffic widget into the disconnect host and show the disconnect page
        if (trafficGraphWidget) {
            trafficGraphWidget->setParent(nullptr);
            disconnectUi.trafficStatsLayout->addWidget(trafficGraphWidget);
        }
        showConnectPage();
        break;
    }
}

void MainWindow::handleVpnError(const QString &message)
{
    auto *progressWidget = connectingUi.progressRingHost->findChild<ConnectionProgressWidget *>();
    if (progressWidget) {
        progressWidget->resetIndicators();
    }
    trafficGraphWidget->resetTraffic();
    showConnectPage();

    QMessageBox::critical(this, "VPN Error", message);
}

void MainWindow::handleVpnByteCountChanged(qulonglong uploadBytes, qulonglong downloadBytes)
{
    if (backend->connectionState() != VpnConnectionState::Connected) {
        return;
    }
    trafficGraphWidget->onByteCountReceived(uploadBytes, downloadBytes);
}

void MainWindow::on_themeToggleButton_clicked()
{
    darkTheme = !darkTheme;
    settings.setValue("theme/dark", darkTheme);
    settings.sync();
    ThemeManager::applyTheme(darkTheme, ui->themeToggleButton);
}

void MainWindow::on_logsButton_clicked()
{
    showConnectionLogs();
}

void MainWindow::handleVpnConnectionInfoChanged(const QString &remote, const QString &remoteAddr, const QString &proto, const QString &localIface, const QString &localIp, const QString &gateway, int mtu)
{
    QString username = QStringLiteral("—");
    QString serverName = QStringLiteral("—");
    QString serverPort = QStringLiteral("—");

    QRegularExpression re(QStringLiteral("(?:(.+)@)?([^:]+)(?::(\\d+))?"));
    const QRegularExpressionMatch match = re.match(remote);
    if (match.hasMatch()) {
        const QString u = match.captured(1);
        const QString h = match.captured(2);
        const QString p = match.captured(3);
        if (!u.isEmpty()) username = u;
        if (!h.isEmpty()) serverName = h;
        if (!p.isEmpty()) serverPort = p;
    } else if (!remote.isEmpty()) {
        serverName = remote;
    }

    const QString tunnel = localIface.isEmpty() ? QStringLiteral("—") : localIface;
    const QString userIp = localIp.isEmpty() ? QStringLiteral("—") : localIp;
    const QString protoText = proto.isEmpty() ? QStringLiteral("—") : proto.toUpper();
    const QString serverIp = remoteAddr.isEmpty() ? QStringLiteral("—") : remoteAddr;

    const QString clientName = (username != QStringLiteral("—") && !username.isEmpty()) ? username : QStringLiteral("—");

    const QString serverNameLine = (!serverName.isEmpty() && serverName != QStringLiteral("—")) ? serverName : QStringLiteral("—");

    // Client detail columns (left: user IP, right: tunnel)
    if (disconnectUi.clientNameLabel) disconnectUi.clientNameLabel->setText(clientName);
    if (disconnectUi.clientDetailsCol1Label) disconnectUi.clientDetailsCol1Label->setText(userIp);
    if (disconnectUi.clientDetailsCol2Label) disconnectUi.clientDetailsCol2Label->setText(tunnel);

    if (disconnectUi.serverNameLabel) disconnectUi.serverNameLabel->setText(serverNameLine);
    if (disconnectUi.serverDetailsCol1Label) disconnectUi.serverDetailsCol1Label->setText(serverIp);
    if (disconnectUi.serverDetailsCol2Label) disconnectUi.serverDetailsCol2Label->setText(serverPort);
    if (disconnectUi.serverDetailsCol3Label) disconnectUi.serverDetailsCol3Label->setText(protoText);
}

void MainWindow::handleDownloadButtonClicked()
{
    QString username;
    QString password;
    if (!getCredentialsFromDialog(this,
                                  QStringLiteral("VPN Credentials"),
                                  QStringLiteral("Enter your kerberos credentials to download the configuration."),
                                  &username, &password)) {
        return;
    }

    certificateService.startCertificateDownload(username, password);
}

void MainWindow::handleConnectButtonClicked()
{
    if (backend->connectionState() == VpnConnectionState::Connected) {
        backend->disconnectVpn();
        return;
    }

    promptForVpnPasswordAndConnect();
}

bool MainWindow::promptForVpnPasswordAndConnect(const QString &message)
{
    if (!QFile::exists(certificateService.downloadedOvpnPath())) {
        QMessageBox::warning(this, "Missing certificate", "Please generate the configuration before attempting to connect.");
        return false;
    }

    if (backend->connectionState() == VpnConnectionState::Connected) {
        return false;
    }

    const QString promptText = message.isEmpty()
        ? QStringLiteral("Enter your kerberos password to connect to the VPN.")
        : QStringLiteral("%1\n\nEnter your kerberos password to connect to the VPN.").arg(message);

    QString password;
    if (!getPasswordFromDialog(this, QStringLiteral("Password Input"), promptText, &password)) {
        return false;
    }

    recentConnectionLogs.clear();
    showConnectingPage();
    backend->connectVpn(certificateService.downloadedOvpnPath(), password);
    return true;
}

void MainWindow::showConnectionLogs()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Connection Logs"));
    dialog.setModal(true);
    dialog.setFixedSize(300, 400);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *viewer = new QPlainTextEdit(&dialog);
    viewer->setReadOnly(true);
    viewer->setPlaceholderText(QStringLiteral("Logs from the most recent VPN socket connection will appear here."));
    viewer->setPlainText(recentConnectionLogs.join(QStringLiteral("\n\n")));
    QFont logFont;
    logFont.setFamily(QStringLiteral("Courier New"));
    logFont.setPointSize(10);
    viewer->setFont(logFont);
    // Wrap long lines to the widget width to avoid horizontal scrolling
    viewer->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    viewer->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Modern thin scrollbar styling for the log viewer
    const QString logScrollStyle = QStringLiteral(
        "QPlainTextEdit { font-family: 'Courier New'; font-size: 10px; }"
        "QScrollBar:vertical { background: transparent; width: 10px; margin: 0px 0px 0px 0px; }"
        "QScrollBar::handle:vertical { background: rgba(0,0,0,0.25); min-height: 20px; border-radius: 5px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { height: 0px; }"
        "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }"
    );
    viewer->setStyleSheet(logScrollStyle);
    layout->addWidget(viewer, 1);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);

    auto *saveButton = new QPushButton(QStringLiteral("Save Logs"), &dialog);
    auto *closeButton = new QPushButton(QStringLiteral("Close"), &dialog);
    saveButton->setCursor(Qt::PointingHandCursor);
    closeButton->setCursor(Qt::PointingHandCursor);
    buttonRow->addWidget(saveButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);

    connect(saveButton, &QPushButton::clicked, &dialog, [this, viewer, &dialog]() {
        const QString defaultName = QStringLiteral("vpn-logs-%1.txt")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
        const QString initialPath = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(defaultName);
        const QString filePath = QFileDialog::getSaveFileName(&dialog, QStringLiteral("Save Logs"), initialPath, QStringLiteral("Text Files (*.txt);;All Files (*)"));
        if (filePath.isEmpty()) {
            return;
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(&dialog, QStringLiteral("Save Failed"), QStringLiteral("Could not write the selected file."));
            return;
        }

        file.write(viewer->toPlainText().toUtf8());
    });

    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    if (trayManager && trayManager->isQuitting()) {
        if (backend->connectionState() == VpnConnectionState::Connected) {
            backend->disconnectVpn();
        }
        event->accept();
        return;
    }

    hide();
    if (trayManager) {
        trayManager->setShowHideActionText(QStringLiteral("Show"));
    }
    event->ignore();
}
