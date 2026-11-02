#include "mainwindow.h"
#include "theme.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("MarkReader");
    QCoreApplication::setApplicationName("MarkReader");
    // Fusion 让自定义复古样式表在 macOS 上表现一致，避免原生控件覆盖按钮和下拉框样式。
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setFont(Theme::uiFont(14));
    MainWindow window;
    window.show();
    return app.exec();
}
