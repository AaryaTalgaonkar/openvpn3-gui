#include <QMainWindow>
#include <memory>
#include "custommessagebox.h"
#include <QCloseEvent>
#include <QSettings>
#include <QStringList>

class QLabel;
class QTimer;
class QSystemTrayIcon;
class QMenu;

#include "ikeystore.h"
#include "ivpnbackend.h"
#include "certificatedownloadservice.h"
#include "connectionprogresswidget.h"
#include "trafficgraphwidget.h"
#include "certificateboxwidget.h"
#include "ui_mainwindow.h"
#include "ui_connectscreen.h"
#include "ui_connectingscreen.h"
#include "ui_disconnectscreen.h"
#include "ui_getstarted.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void handleGetStartedClicked();
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
    Ui::ConnectScreen connectUi;
    Ui::ConnectingScreen connectingUi;
    Ui::DisconnectScreen disconnectUi;
    Ui::GetStarted getStartedUi;
    std::unique_ptr<IVpnBackend> backend;
    std::unique_ptr<IKeyStore> keystore;
    CertificateDownloadService certificateService;
    TrafficGraphWidget *trafficGraphWidget = nullptr;
    QStringList recentConnectionLogs;
    QSettings settings;

    CertificateBoxWidget *certBox = nullptr;

    void setInitialFlow();
    void setConnectFlow();
    void loadSavedCertificateState();
    void applyTheme(bool dark);
    void showConnectPage();
    void showConnectingPage();
    bool promptForVpnPasswordAndConnect(const QString &message = QString());
    void updateCertificateInfoBox();
    void showConnectionLogs();
    bool darkTheme = false;
    QSystemTrayIcon *trayIcon = nullptr;
    QMenu *trayMenu = nullptr;
    QAction *trayShowHideAction = nullptr;
    bool quitting = false;
    void closeEvent(QCloseEvent *event) override;
    void setupSystemTray();
    void toggleMainWindowVisibility();
};