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
    static QSharedMemory *sharedMem = nullptr;
    if (!sharedMem) {
        sharedMem = new QSharedMemory(kSharedMemoryKey);
    }

    if (sharedMem->attach()) {
        QLocalSocket socket;
        socket.connectToServer(kLocalServerName);
        if (socket.waitForConnected(1000)) {
            socket.write("raise");
            socket.waitForBytesWritten(1000);
            socket.waitForDisconnected(1000);
        }
        return false;
    }

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

    m_localServer = new QLocalServer(this);
    QLocalServer::removeServer(kLocalServerName);
    if (m_localServer->listen(kLocalServerName)) {
        connect(m_localServer, &QLocalServer::newConnection, this, [this]() {
            QLocalSocket *clientSocket = m_localServer->nextPendingConnection();
            if (clientSocket) {
                connect(clientSocket, &QLocalSocket::readyRead, this, [this, clientSocket]() {
                    QByteArray data = clientSocket->readAll();
                    if (data.trimmed() == "raise") {
                        if (m_mainWindow) {
                            m_mainWindow->show();
                            m_mainWindow->raise();
                            m_mainWindow->activateWindow();
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
        m_trayMenu = new QMenu();
    }

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
    if (obj == m_mainWindow && event->type() == QEvent::Close) {
        auto *closeEvent = static_cast<QCloseEvent *>(event);
        if (!m_quitting) {
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