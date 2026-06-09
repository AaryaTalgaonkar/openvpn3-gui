#include "ivpnbackend.h"
#include <QProcess>

class LinuxVpnBackend : public IVpnBackend
{
    Q_OBJECT
public:
    explicit LinuxVpnBackend(QObject *parent = nullptr);

    void connectVpn(const QString &ovpnPath,
                    const QString &password) override;

    void disconnectVpn() override;
    VpnConnectionState connectionState() const override;

private slots:
    void onLogOutput();

private:
    QProcess logProcess;
    QProcess sessionStart;

    QString configPath;
    VpnConnectionState connectedState = VpnConnectionState::Disconnected;
};
