#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(&vpn, &VpnController::statusChanged,
            ui->connectButton, &QPushButton::setText);

    connect(&vpn, &VpnController::errorOccurred,
            this, [&](const QString &msg) {
                QMessageBox::critical(this, "VPN Error", msg);
            });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_browseOvpn_clicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Select File", {}, "*.ovpn");
    if (!path.isEmpty())
        ui->ovpnPathEdit->setText(path);
}

void MainWindow::on_connectButton_clicked()
{
    if (!vpn.isConnected()) {
        vpn.connectVpn(
            ui->ovpnPathEdit->text(),
            ui->usernameEdit->text(),
            ui->passwordEdit->text()
            );
    } else {
        vpn.disconnectVpn();
    }
}
