#include "systemtraymanager.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QWidget>

SystemTrayManager::SystemTrayManager(QWidget *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
}

SystemTrayManager::~SystemTrayManager()
{
    delete m_trayMenu;
}

void SystemTrayManager::setup()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    m_trayIcon = new QSystemTrayIcon(QIcon(QStringLiteral(":/img/logo.png")), this);
    m_trayIcon->setToolTip(QStringLiteral("IITDelhiVPN"));

    m_trayMenu = new QMenu(qobject_cast<QWidget *>(m_mainWindow));
    if (!m_trayMenu) {
        // Fallback: create menu without parent widget
        m_trayMenu = new QMenu();
    }

    m_showHideAction = m_trayMenu->addAction(QStringLiteral("Hide"));
    connect(m_showHideAction, &QAction::triggered, this, &SystemTrayManager::showHideRequested);

    m_trayMenu->addSeparator();

    QAction *quitAction = m_trayMenu->addAction(QStringLiteral("Quit"));
    connect(quitAction, &QAction::triggered, this, &SystemTrayManager::quitRequested);

    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::DoubleClick) {
                    emit showHideRequested();
                }
            });

    m_trayIcon->show();
}

void SystemTrayManager::setShowHideActionText(const QString &text)
{
    if (m_showHideAction) {
        m_showHideAction->setText(text);
    }
}

bool SystemTrayManager::isQuitting() const
{
    return m_quitting;
}

void SystemTrayManager::setQuitting(bool quitting)
{
    m_quitting = quitting;
}