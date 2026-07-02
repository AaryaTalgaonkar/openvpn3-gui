#ifndef SYSTEMTRAYMANAGER_H
#define SYSTEMTRAYMANAGER_H

#include <QObject>

class QWidget;
class QSystemTrayIcon;
class QMenu;
class QAction;
class QLocalServer;
class QEvent;

class SystemTrayManager : public QObject
{
    Q_OBJECT

public:
    explicit SystemTrayManager(QWidget *mainWindow, QObject *parent = nullptr);
    ~SystemTrayManager() override;

    static bool ensureSingleInstance();

    void setup();
    bool isQuitting() const;

signals:
    void quitRequested();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void toggleVisibility();

    QWidget *m_mainWindow;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_showHideAction = nullptr;
    QLocalServer *m_localServer = nullptr;
    bool m_quitting = false;
};

#endif // SYSTEMTRAYMANAGER_H