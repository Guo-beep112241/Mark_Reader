#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("MarkReader");
    QCoreApplication::setApplicationName("MarkReader");
    MainWindow window;
    window.show();
    return app.exec();
}
