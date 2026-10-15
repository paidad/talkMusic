#include "app/mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("TalkMusic");
    QCoreApplication::setApplicationName("TalkMusic");
    MainWindow w;
    w.show();
    return a.exec();
}
