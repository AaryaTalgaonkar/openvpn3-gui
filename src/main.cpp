#include "mainwindow.h"

#include <QApplication>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName("IITDelhiVPN");
    QApplication::setDesktopFileName("iitdelhivpn");
    QFontDatabase::addApplicationFont(":/fonts/TitilliumWeb-Regular.ttf");
    QApplication::setFont(QFont("Titillium Web"));
    a.setWindowIcon(QIcon(":/img/logo.png"));
    MainWindow w;
    w.show();
    return a.exec();
}
