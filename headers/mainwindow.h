#include <QMainWindow>
#include <memory>
#include "custommessagebox.h"
#include <QCloseEvent>
#include <QSettings>
#include <QVector>
#include <QStringList>
#include <QElapsedTimer>
#include <QDateTime>

class QByteArray;
class QNetworkReply;
class QUrl;
class QLabel;
class QTimer;
class QProgressBar;

#include "ivpnbackend.h"
#include "certificatedownloadservice.h"
#include "connectionprogresswidget.h"
#include "trafficgraphwidget.h"
#include "ui_mainwindow.h"
#include "ui_downloadscreen.h"
#include "ui_connectscreen.h"
#include "ui_connectingscreen.h"
#include "ui_disconnectscreen.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void handleDownloadButtonClicked();
    void handleConnectButtonClicked();
    void on_themeToggleButton_clicked();
    void on_logsButton_clicked();
    void handleVpnStateChanged(const QString &state);
    void handleVpnConnectionStateChanged(VpnConnectionState state);
    void handleVpnError(const QString &message);
    void handleVpnByteCountChanged(qulonglong uploadBytes, qulonglong downloadBytes);
    void handleVpnConnectionInfoChanged(const QString &remote, const QString &remoteAddr, const QString &proto, const QString &localIface, const QString &localIp, const QString &gateway, int mtu);


private:
    Ui::MainWindow *ui;
    Ui::DownloadScreen downloadUi;
    Ui::ConnectScreen connectUi;
    Ui::ConnectingScreen connectingUi;
    Ui::DisconnectScreen disconnectUi;
    std::unique_ptr<IVpnBackend> backend;
    CertificateDownloadService certificateService;
    ConnectionProgressWidget *progressWidget = nullptr;
    TrafficGraphWidget *trafficGraphWidget = nullptr;
    QVector<QLabel *> stepStatusLabels;
    QTimer *spinnerTimer = nullptr;
    QStringList spinnerFrames;
    QStringList recentConnectionLogs;
    int spinnerFrameIndex = 0;
    int activeStepIndex = -1;
    QElapsedTimer trafficTimer;
    qulonglong lastUploadBytes = 0;
    qulonglong lastDownloadBytes = 0;
    qint64 lastTrafficSampleMs = 0;
    bool trafficSampleInitialized = false;
    QSettings settings;

    QDateTime certEffectiveDate;
    QDateTime certExpiryDate;

    void setInitialFlow();
    void setConnectFlow();
    void loadSavedCertificateState();
    void saveCertificateState();
    void startCertificateDownload();
    void handleInitialResponse(class QNetworkReply *reply);
    void handleDownloadResponse(class QNetworkReply *reply);
    void sendCertificateRequest(const QUrl &url, const QByteArray &body);
    QString extractHiddenField(const QString &html, const QString &name) const;
    QString makeDownloadPath(const QString &username) const;
    void applyTheme(bool dark);
    void setupConnectingScreen();
    void showConnectPage();
    void showConnectingPage();
    bool promptForVpnPasswordAndConnect(const QString &message = QString());
    void resetConnectingIndicators();
    void resetTrafficIndicators();
    QString formatThroughput(qreal bytesPerSecond) const;
    void updateConnectionStep(const QString &state);
    void updateCertificateInfoBox();
    void handleGoogleTimeReply(QNetworkReply *reply);
    void updateCertValidityDisplay(const QDateTime &now);
    void refreshSpinnerFrame();
    int stepIndexForState(const QString &state) const;
    void updateStepRows(int currentIndex);
    void showConnectionLogs();
    bool darkTheme = false;
    void closeEvent(QCloseEvent *event) override;
};