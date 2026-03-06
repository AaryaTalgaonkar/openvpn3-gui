#include "vpncontroller.h"

#ifdef Q_OS_LINUX
#include "linuxvpnbackend.h"
#elif defined(Q_OS_WIN) || defined(Q_OS_MAC)
#include "winmacvpnbackend.h"
#endif

VpnController::VpnController(QObject *parent)
    : QObject(parent)
{
    #ifdef Q_OS_LINUX
        backend = std::make_unique<LinuxVpnBackend>(this);
    #elif defined(Q_OS_WIN) || defined(Q_OS_MAC)
        backend = std::make_unique<WinMacVpnBackend>(this);
    #endif

    // 🔁 Forward backend signals to UI
    connect(backend.get(), &IVpnBackend::connected,
            this, &VpnController::connected);

    connect(backend.get(), &IVpnBackend::disconnected,
            this, &VpnController::disconnected);

    connect(backend.get(), &IVpnBackend::errorOccurred,
            this, &VpnController::errorOccurred);

    connect(backend.get(), &IVpnBackend::statusChanged,
            this, &VpnController::statusChanged);
}

void VpnController::connectVpn(const QString &ovpnPath,
                               const QString &username,
                               const QString &password)
{
    backend->connectVpn(ovpnPath, username, password);
}

void VpnController::disconnectVpn()
{
    backend->disconnectVpn();
}

bool VpnController::isConnected() const
{
    return backend->isConnected();
}
