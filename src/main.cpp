#include "mainwindow.h"
#include "theme.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("MarkReader");
    QCoreApplication::setApplicationName("MarkReader");
    // 与 application bundle 的 AppIcon.icns 使用同一张源图，主窗口和系统对话框共用。
    QApplication::setWindowIcon(QIcon(":/theme/app-icon.png"));
    // Fusion 让自定义复古样式表在 macOS 上表现一致，避免原生控件覆盖按钮和下拉框样式。
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setFont(Theme::uiFont(14));
    MainWindow window;
    window.show();
    return app.exec();
}
