#include "systemtraymanager.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QSharedMemory>
#include <QSystemTrayIcon>
#include <QWidget>

static const char *kSharedMemoryKey = "IITDelhiVPN-SingleInstance";
static const char *kLocalServerName = "IITDelhiVPN-LocalServer";

SystemTrayManager::SystemTrayManager(QWidget *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
    // Install event filter to intercept close events on the main window
    if (m_mainWindow) {
        m_mainWindow->installEventFilter(this);
    }
}

SystemTrayManager::~SystemTrayManager()
{
    delete m_trayMenu;
}

bool SystemTrayManager::ensureSingleInstance()
{
    // Use a static pointer so the QSharedMemory persists for the app's lifetime.
    // If it were a local variable, it would go out of scope and release the
    // shared memory segment, defeating the single-instance check.
    static QSharedMemory *sharedMem = nullptr;
    if (!sharedMem) {
        sharedMem = new QSharedMemory(kSharedMemoryKey);
    }

    // Try to attach to existing shared memory
    if (sharedMem->attach()) {
        // Another instance is already running — tell it to show itself
        QLocalSocket socket;
        socket.connectToServer(kLocalServerName);
        if (socket.waitForConnected(1000)) {
            socket.write("raise");
            socket.waitForBytesWritten(1000);
            socket.waitForDisconnected(1000);
        }
        return false; // Signal that we should not continue
    }

    // We are the first instance — create the shared memory
    if (!sharedMem->create(1)) {
        return false;
    }

    return true;
}

void SystemTrayManager::setup()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    // Set up local server to receive "raise" commands from future instances
    m_localServer = new QLocalServer(this);
    // Remove any leftover server from a previous crash
    QLocalServer::removeServer(kLocalServerName);
    if (m_localServer->listen(kLocalServerName)) {
        connect(m_localServer, &QLocalServer::newConnection, this, [this]() {
            QLocalSocket *clientSocket = m_localServer->nextPendingConnection();
            if (clientSocket) {
                connect(clientSocket, &QLocalSocket::readyRead, this, [this, clientSocket]() {
                    QByteArray data = clientSocket->readAll();
                    if (data.trimmed() == "raise") {
                        // Bring the main window to the front
                        if (m_mainWindow) {
                            m_mainWindow->show();
                            m_mainWindow->raise();
                            m_mainWindow->activateWindow();
                            // Update tray menu text if showing
                            if (m_showHideAction) {
                                m_showHideAction->setText(QStringLiteral("Hide"));
                            }
                        }
                    }
                    clientSocket->disconnectFromServer();
                });
                connect(clientSocket, &QLocalSocket::disconnected, clientSocket, &QLocalSocket::deleteLater);
            }
        });
    }

    m_trayIcon = new QSystemTrayIcon(QIcon(QStringLiteral(":/img/logo.png")), this);
    m_trayIcon->setToolTip(QStringLiteral("IITDelhiVPN"));

    m_trayMenu = new QMenu(qobject_cast<QWidget *>(m_mainWindow));
    if (!m_trayMenu) {
        // Fallback: create menu without parent widget
        m_trayMenu = new QMenu();
    }

    // Show/Hide action — managed entirely by this class
    m_showHideAction = m_trayMenu->addAction(QStringLiteral("Hide"));
    connect(m_showHideAction, &QAction::triggered, this, &SystemTrayManager::toggleVisibility);

    m_trayMenu->addSeparator();

    QAction *quitAction = m_trayMenu->addAction(QStringLiteral("Quit"));
    connect(quitAction, &QAction::triggered, this, [this]() {
        m_quitting = true;
        emit quitRequested();
        QApplication::quit();
    });

    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::DoubleClick) {
                    toggleVisibility();
                }
            });

    m_trayIcon->show();
}

bool SystemTrayManager::isQuitting() const
{
    return m_quitting;
}

bool SystemTrayManager::eventFilter(QObject *obj, QEvent *event)
{
    // Intercept close events on the main window — hide to tray instead
    if (obj == m_mainWindow && event->type() == QEvent::Close) {
        auto *closeEvent = static_cast<QCloseEvent *>(event);
        if (!m_quitting) {
            // Hide instead of closing
            toggleVisibility();
            closeEvent->ignore();
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void SystemTrayManager::toggleVisibility()
{
    if (!m_mainWindow) {
        return;
    }

    if (m_mainWindow->isVisible()) {
        // Hide all modal dialogs as well (e.g. password prompt, error messages)
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (widget != m_mainWindow && widget->isModal() && widget->isVisible()) {
                widget->hide();
            }
        }
        m_mainWindow->hide();
        if (m_showHideAction) {
            m_showHideAction->setText(QStringLiteral("Show"));
        }
    } else {
        m_mainWindow->show();
        m_mainWindow->raise();
        m_mainWindow->activateWindow();
        if (m_showHideAction) {
            m_showHideAction->setText(QStringLiteral("Hide"));
        }
    }
}