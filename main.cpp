#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("TalkMusic");
    QCoreApplication::setApplicationName("TalkMusic");
    MainWindow w;
    w.show();
    return a.exec();
}
