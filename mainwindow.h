#include <QMainWindow>
#include <QFileDialog>
#include <QMessageBox>
#include "vpncontroller.h"
#include "./ui_mainwindow.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_browseOvpn_clicked();
    void on_connectButton_clicked();

private:
    Ui::MainWindow *ui;
    VpnController vpn;
};
