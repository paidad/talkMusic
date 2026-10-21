#include "app/mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    // WebEngine 的图形上下文共享属性必须在创建 QApplication 前设置。
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication a(argc, argv);
    // 组织名与应用名决定 QSettings 和默认应用数据目录的命名空间。
    QCoreApplication::setOrganizationName("TalkMusic");
    QCoreApplication::setApplicationName("TalkMusic");
    MainWindow w;
    w.show();
    return a.exec();
}
