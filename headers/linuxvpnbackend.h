#include "ivpnbackend.h"
#include <QProcess>

class LinuxVpnBackend : public IVpnBackend
{
    Q_OBJECT
public:
    explicit LinuxVpnBackend(QObject *parent = nullptr);

    void connectVpn(const QString &ovpnPath,
                    const QString &username,
                    const QString &password) override;

    void disconnectVpn() override;
    bool isConnected() const override;

private slots:
    void onLogOutput();

private:
    QProcess logProcess;
    QProcess sessionStart;

    QString configPath;
    bool connectedState = false;
};
