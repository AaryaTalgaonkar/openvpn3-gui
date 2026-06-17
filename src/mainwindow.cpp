#include "mainwindow.h"

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

#ifdef Q_OS_LINUX
#include "linuxvpnbackend.h"
#elif defined(Q_OS_WIN) || defined(Q_OS_MAC)
#include "winmacvpnbackend.h"
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

constexpr auto kLoginUrl = "https://newcert.iitd.ac.in/cgi-bin/usermanage/vpn3cert.cgi";
constexpr auto kDownloadUrl = "https://newcert.iitd.ac.in/cgi-bin/usermanage/getvpn3cert.cgi";
constexpr int kVpnPasswordDialogWidth = 250;
constexpr auto kConnectingAccentColor = "#16a34a";

void setPointingHandCursorForButtons(QWidget *widget)
{
    for (QPushButton *button : widget->findChildren<QPushButton *>()) {
        if (button) {
            button->setCursor(Qt::PointingHandCursor);
        }
    }
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

bool isConnectivityFailure(QNetworkReply::NetworkError error)
{
    switch (error) {
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::UnknownNetworkError:
        return true;
    default:
        return false;
    }
}

bool containsInsensitive(const QString &html, const char *needle)
{
    return html.contains(QString::fromUtf8(needle), Qt::CaseInsensitive);
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , settings("IIT Delhi", "IITDelhiVPN")
{
    ui->setupUi(this);
    downloadUi.setupUi(ui->downloadPage);
    connectUi.setupUi(ui->connectPage);
    connectingUi.setupUi(ui->connectingPage);
    disconnectUi.setupUi(ui->disconnectPage);

#ifdef Q_OS_LINUX
    backend = std::make_unique<LinuxVpnBackend>(this);
#elif defined(Q_OS_WIN) || defined(Q_OS_MAC)
    backend = std::make_unique<WinMacVpnBackend>(this);
#endif

    setupConnectingScreen();

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
    applyTheme(darkTheme);

    auto makeEmojiIcon = [](const QString &emoji, int px) {
        QFont f;
        f.setPointSize(px);
        QFontMetrics fm(f);
        const int w = px;
        const int h = px;
        QPixmap pix(w, h);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setFont(f);
        p.setPen(QPen(QColor("#c21717")));
        p.drawText(pix.rect(), Qt::AlignCenter, emoji);
        p.end();
        return QIcon(pix);
    };

    if (downloadUi.usernameEdit) {
        downloadUi.usernameEdit->addAction(makeEmojiIcon(QString::fromUtf8("👤"), 14), QLineEdit::LeadingPosition);
        downloadUi.usernameEdit->setStyleSheet("padding-left: 28px;");
    }
    if (downloadUi.passwordEdit) {
        downloadUi.passwordEdit->addAction(makeEmojiIcon(QString::fromUtf8("🔒"), 14), QLineEdit::LeadingPosition);
        downloadUi.passwordEdit->setStyleSheet("padding-left: 28px;");
    }
    if (downloadUi.userIcon) downloadUi.userIcon->setVisible(false);
    if (downloadUi.passIcon) downloadUi.passIcon->setVisible(false);


    connectUi.connectButton->setCheckable(false);
    connectUi.connectButton->setText(QStringLiteral("⏻")); // Set to power-on emoji
    downloadUi.downloadButton->setEnabled(true);
    connect(downloadUi.downloadButton, &QPushButton::clicked,
            this, &MainWindow::handleDownloadButtonClicked);
        connect(connectUi.connectButton, &QPushButton::clicked,
            this, &MainWindow::handleConnectButtonClicked);
        connect(disconnectUi.disconnectButton, &QPushButton::clicked,
            this, &MainWindow::handleConnectButtonClicked);
    loadSavedCertificateState();

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
                downloadUi.downloadButton->setEnabled(!busy);
                downloadUi.downloadButton->setText(busy ? "Downloading..." : "Download");
            });

    connect(&certificateService, &CertificateDownloadService::statusMessage,
            this, [this](const QString &message) {
                ui->statusbar->showMessage(message, 5000);
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

    spinnerTimer = new QTimer(this);
    connect(spinnerTimer, &QTimer::timeout, this, &MainWindow::refreshSpinnerFrame);

    loadSavedCertificateState();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setInitialFlow()
{
    ui->screenStack->setCurrentWidget(ui->downloadPage);
    downloadUi.contentFrame->setStyleSheet(QStringLiteral("QFrame#contentFrame { border: 1px solid rgba(0,0,0,0.06); border-radius: 10px; padding: 10px; }"));
    downloadUi.downloadButton->setEnabled(true);
    downloadUi.downloadButton->setText("Download");
    connectUi.connectButton->setText(QStringLiteral("⏻")); // Set to power-on emoji
    disconnectUi.disconnectButton->setText(QStringLiteral("⏻"));
    disconnectUi.connectedTrafficFrame->setVisible(false);
    resetTrafficIndicators();
}

void MainWindow::setConnectFlow()
{
    ui->screenStack->setCurrentWidget(ui->connectPage);
    showConnectPage();
}

void MainWindow::loadSavedCertificateState()
{
    certificateService.loadSavedCertificateState();
}

void MainWindow::applyTheme(bool dark)
{
    const QString accent = "#c21717";
    if (dark) {
        qApp->setStyleSheet(QStringLiteral(R"(
            QWidget { background-color: #0e0f10; color: #e6e6e6; }
            QLabel { color: #e6e6e6; }
            QLineEdit, QPlainTextEdit, QTextEdit, QComboBox { background-color: #1e1e1e; color: #e6e6e6; border: 1px solid #2b2b2b; border-radius: 4px; padding: 4px; }
            QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QComboBox:focus { border: 1px solid %1; }
            QPushButton { background-color: #1b1b1b; color: #e6e6e6; border: 1px solid #2f2f2f; border-radius: 6px; padding: 6px 10px; }
            QPushButton:hover { background-color: rgba(194,23,23,0.10); }
            QPushButton:pressed { background-color: rgba(194,23,23,0.18); }
            QPushButton#themeToggleButton { border-radius: 14px; background-color: transparent; border: 1px solid rgba(255,255,255,0.06); }
            QPushButton#themeToggleButton:hover { background-color: rgba(194,23,23,0.12); }
            QPushButton#logsButton { border-radius: 14px; background-color: transparent; border: 1px solid rgba(255,255,255,0.06); }
            QPushButton#logsButton:hover { background-color: rgba(194,23,23,0.12); }
            QStatusBar { background: transparent; color: #bdbdbd; }
            QDialog, QMessageBox { background-color: #121212; color: #e6e6e6; }
            QMessageBox QPushButton { background-color: #2b2b2b; color: #e6e6e6; border: 1px solid #363636; border-radius: 6px; padding: 4px 8px; }
        )").arg(accent));
        ui->themeToggleButton->setText("☀");
    } else {
        qApp->setStyleSheet(QStringLiteral(R"(
            QWidget { background-color: #ffffff; color: #1f1f1f; }
            QLabel { color: #1f1f1f; }
            QLineEdit, QPlainTextEdit, QTextEdit, QComboBox { background-color: #ffffff; color: #1f1f1f; border: 1px solid #dedede; border-radius: 4px; padding: 4px; }
            QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QComboBox:focus { border: 1px solid %1; }
            QPushButton { background-color: #f5f5f5; color: #1f1f1f; border: 1px solid #e6e6e6; border-radius: 6px; padding: 6px 10px; }
            QPushButton:hover { background-color: rgba(194,23,23,0.08); }
            QPushButton#themeToggleButton { border-radius: 14px; background-color: transparent; border: 1px solid rgba(0,0,0,0.06); }
            QPushButton#themeToggleButton:hover { background-color: rgba(194,23,23,0.12); }
            QPushButton#logsButton { border-radius: 14px; background-color: transparent; border: 1px solid rgba(0,0,0,0.06); }
            QPushButton#logsButton:hover { background-color: rgba(194,23,23,0.12); }
            QStatusBar { background: transparent; color: #666666; }
            QDialog, QMessageBox { background-color: #ffffff; color: #1f1f1f; }
            QMessageBox QPushButton { background-color: #ffffff; color: #1f1f1f; border: 1px solid #cccccc; border-radius: 6px; padding: 4px 8px; }
        )").arg(accent));
        ui->themeToggleButton->setText("☾");

    }
}

    void MainWindow::setupConnectingScreen()
    {
        progressWidget = new ConnectionProgressWidget(connectingUi.progressRingHost);
        progressWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        connectingUi.progressRingHostLayout->addWidget(progressWidget, 0, Qt::AlignCenter);

        while (QLayoutItem *item = connectingUi.stepsLayout->takeAt(0)) {
            delete item->widget();
            delete item;
        }
        stepStatusLabels.clear();

        for (int i = 0; i < kConnectionStepCount; ++i) {
            const auto &step = kConnectionSteps[i];
            auto *row = new QWidget(connectingUi.stepsFrame);
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(10);

            auto *iconLabel = new QLabel(QString::fromUtf8(step.icon), row);
            iconLabel->setFixedWidth(24);
            iconLabel->setAlignment(Qt::AlignCenter);

            auto *titleLabel = new QLabel(QString::fromUtf8(step.label), row);
            titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

            auto *statusLabel = new QLabel(row);
            statusLabel->setFixedSize(24, 24);
            statusLabel->setAlignment(Qt::AlignCenter);

            rowLayout->addWidget(iconLabel);
            rowLayout->addWidget(titleLabel, 1);
            rowLayout->addWidget(statusLabel);
            connectingUi.stepsLayout->addWidget(row);
            stepStatusLabels.push_back(statusLabel);
        }

        resetConnectingIndicators();
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
            resetTrafficIndicators();
        }
        connectUi.connectButton->setEnabled(true);
        disconnectUi.disconnectButton->setEnabled(true);
        ui->statusbar->showMessage(backend->connectionState() == VpnConnectionState::Connected ? QStringLiteral("Status: Connected") : QStringLiteral("Status: Disconnected"));
    }

    void MainWindow::showConnectingPage()
    {
        ui->screenStack->setCurrentWidget(ui->connectingPage);
        resetConnectingIndicators();
        ui->statusbar->showMessage(QStringLiteral("Status: Connecting"));
        spinnerTimer->start(140);
    }

    void MainWindow::resetConnectingIndicators()
    {
        spinnerFrameIndex = 0;
        activeStepIndex = 0;
        if (progressWidget) {
            progressWidget->setProgressStep(0, stepStatusLabels.size());
        }
        updateStepRows(0);
    }

    void MainWindow::resetTrafficIndicators()
    {
        trafficTimer.invalidate();
        lastUploadBytes = 0;
        lastDownloadBytes = 0;
        lastTrafficSampleMs = 0;
        trafficSampleInitialized = false;
        if (trafficGraphWidget) {
            trafficGraphWidget->clearSamples();
        }
        // speeds are shown on the graph overlay; no separate stat labels needed
    }

    QString MainWindow::formatThroughput(qreal bytesPerSecond) const
    {
        const qreal value = qMax<qreal>(0.0, bytesPerSecond);
        const char *units[] = {"B/s", "KB/s", "MB/s", "GB/s"};
        int unitIndex = 0;
        qreal scaled = value;
        while (scaled >= 1024.0 && unitIndex < 3) {
            scaled /= 1024.0;
            ++unitIndex;
        }
        return QStringLiteral("%1 %2").arg(QString::number(scaled, 'f', scaled < 10.0 ? 1 : 0), units[unitIndex]);
    }

    int MainWindow::stepIndexForState(const QString &state) const
    {
        const QString normalized = state.trimmed().toUpper();
        for (int index = 0; index < kConnectionStepCount; ++index) {
            if (normalized == QLatin1String(kConnectionSteps[index].state)) {
                return index;
            }
        }
        return -1;
    }

    void MainWindow::updateStepRows(int currentIndex)
    {
        if (stepStatusLabels.isEmpty()) {
            return;
        }

        const int lastIndex = stepStatusLabels.size() - 1;
        const int boundedIndex = qBound(0, currentIndex, lastIndex);
        activeStepIndex = boundedIndex;

        for (int index = 0; index < stepStatusLabels.size(); ++index) {
            QLabel *statusLabel = stepStatusLabels.at(index);
            if (!statusLabel) {
                continue;
            }

            if (index < boundedIndex) {
                statusLabel->setText(QString::fromUtf8("✓"));
                statusLabel->setStyleSheet(QStringLiteral("QLabel { border: 1px solid rgba(34,197,94,0.75); border-radius: 12px; color: #16a34a; background-color: rgba(34,197,94,0.12); font-weight: bold; }"));
            } else if (index == boundedIndex) {
                if (boundedIndex >= lastIndex) {
                    statusLabel->setText(QString::fromUtf8("✓"));
                    statusLabel->setStyleSheet(QStringLiteral("QLabel { border: 1px solid rgba(34,197,94,0.75); border-radius: 12px; color: #16a34a; background-color: rgba(34,197,94,0.12); font-weight: bold; }"));
                } else {
                    const QString spinner = spinnerFrames.isEmpty() ? QStringLiteral("◐") : spinnerFrames.at(spinnerFrameIndex % spinnerFrames.size());
                    statusLabel->setText(spinner);
                    statusLabel->setStyleSheet(QStringLiteral("QLabel { border: 1px solid rgba(34,197,94,0.85); border-radius: 12px; color: %1; background-color: rgba(34,197,94,0.08); font-weight: bold; }").arg(kConnectingAccentColor));
                }
            } else {
                statusLabel->setText(QString());
                statusLabel->setStyleSheet(QStringLiteral("QLabel { border: 1px solid rgba(0,0,0,0.14); border-radius: 12px; color: rgba(0,0,0,0.35); background-color: transparent; }"));
            }
        }

        if (progressWidget) {
            progressWidget->setProgressStep(boundedIndex, stepStatusLabels.size());
        }

        if (boundedIndex >= lastIndex) {
            spinnerTimer->stop();
        }
    }

    void MainWindow::refreshSpinnerFrame()
    {
        if (spinnerFrames.isEmpty() || activeStepIndex < 0 || activeStepIndex >= stepStatusLabels.size() - 1) {
            return;
        }

        spinnerFrameIndex = (spinnerFrameIndex + 1) % spinnerFrames.size();
        updateStepRows(activeStepIndex);
    }

    void MainWindow::updateConnectionStep(const QString &state)
    {
        const int index = stepIndexForState(state);
        if (index < 0) {
            return;
        }

        if (spinnerFrames.isEmpty()) {
            spinnerFrames = {QString::fromUtf8("◐"), QString::fromUtf8("◓"), QString::fromUtf8("◑"), QString::fromUtf8("◒")};
        }

        ui->statusbar->showMessage(QStringLiteral("Status: Connecting"));
        if (index < kConnectionStepCount - 1) {
            if (!spinnerTimer->isActive()) {
                spinnerTimer->start(140);
            }
        }
        updateStepRows(index);
    }

    void MainWindow::handleVpnStateChanged(const QString &state)
    {
        updateConnectionStep(state);
        if (stepIndexForState(state) >= 0 && ui->screenStack->currentWidget() != ui->connectingPage) {
            showConnectingPage();
        }
    }

    void MainWindow::handleVpnConnectionStateChanged(VpnConnectionState state)
    {
        switch (state) {
        case VpnConnectionState::Disconnected:
            spinnerTimer->stop();
            resetConnectingIndicators();
            resetTrafficIndicators();
            showConnectPage();
            break;

        case VpnConnectionState::Connecting:
            showConnectingPage();
            break;

        case VpnConnectionState::Connected:
            spinnerTimer->stop();
            if (progressWidget) {
                progressWidget->setProgressStep(kConnectionStepCount - 1, kConnectionStepCount);
            }
            updateStepRows(kConnectionStepCount - 1);
            resetTrafficIndicators();
            trafficTimer.start();
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
        spinnerTimer->stop();
        resetConnectingIndicators();
        resetTrafficIndicators();
        showConnectPage();

        QMessageBox::critical(this, "VPN Error", message);
    }

    void MainWindow::handleVpnByteCountChanged(qulonglong uploadBytes, qulonglong downloadBytes)
    {
        if (backend->connectionState() != VpnConnectionState::Connected) {
            return;
        }

        if (!trafficTimer.isValid()) {
            trafficTimer.start();
        }

        const qint64 nowMs = trafficTimer.elapsed();

        if (!trafficSampleInitialized) {
            trafficSampleInitialized = true;
            lastUploadBytes = uploadBytes;
            lastDownloadBytes = downloadBytes;
            lastTrafficSampleMs = nowMs;
            if (trafficGraphWidget) {
                trafficGraphWidget->appendSample(0.0, 0.0, 0.0);
            }
            return;
        }

        const qreal elapsedSeconds = qMax<qreal>(0.001, (nowMs - lastTrafficSampleMs) / 1000.0);
        const qulonglong uploadDeltaBytes = uploadBytes >= lastUploadBytes ? (uploadBytes - lastUploadBytes) : 0;
        const qulonglong downloadDeltaBytes = downloadBytes >= lastDownloadBytes ? (downloadBytes - lastDownloadBytes) : 0;
        const qreal uploadSpeed = static_cast<qreal>(uploadDeltaBytes) / elapsedSeconds;
        const qreal downloadSpeed = static_cast<qreal>(downloadDeltaBytes) / elapsedSeconds;

        lastUploadBytes = uploadBytes;
        lastDownloadBytes = downloadBytes;
        lastTrafficSampleMs = nowMs;

        if (trafficGraphWidget) {
            trafficGraphWidget->appendSample(nowMs / 1000.0, uploadSpeed, downloadSpeed);
        }

    }

void MainWindow::on_themeToggleButton_clicked()
{
    darkTheme = !darkTheme;
    settings.setValue("theme/dark", darkTheme);
    settings.sync();
    applyTheme(darkTheme);
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
    certificateService.startCertificateDownload(downloadUi.usernameEdit->text(), downloadUi.passwordEdit->text());
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
    if (certificateService.downloadedOvpnPath().isEmpty()) {
        QMessageBox::warning(this, "Missing certificate", "Download the OVPN file first.");
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
    if (backend->connectionState() == VpnConnectionState::Connected) {
        backend->disconnectVpn();
    }
        event->accept();
}