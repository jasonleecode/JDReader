#include "mainwindow.h"
#include "storage/Database.h"

#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QtSvg/QSvgRenderer>
#include <QPainter>
#include <QPixmap>

// Build a multi-resolution QIcon from the embedded SVG.
// Qt's QIcon::fromTheme fallback is also set so desktop environments
// that support icon themes can use a higher-quality version if available.
static QIcon makeAppIcon() {
    QSvgRenderer renderer(QString(":/resources/icons/app.svg"));

    QIcon icon;
    for (int sz : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pm(sz, sz);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        renderer.render(&p);
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

int main(int argc, char *argv[]) {
    // Must be set before QApplication for WebEngine
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // Remote debugging: open chrome://inspect in Chrome, or http://localhost:9222
    // Set env var JDREADER_NO_DEVTOOLS=1 to disable
    if (qgetenv("JDREADER_NO_DEVTOOLS") != "1") {
        qputenv("QTWEBENGINE_REMOTE_DEBUGGING", "9222");
    }

    QApplication app(argc, argv);
    app.setApplicationName("JDReader");
    app.setOrganizationName("JDReader");
    app.setApplicationVersion("1.0.0");

    // Link this process to the .desktop file so GNOME Shell shows the
    // correct icon in the taskbar (matches StartupWMClass=JDReader).
    app.setDesktopFileName("jdreader");

    const QIcon appIcon = makeAppIcon();
    app.setWindowIcon(appIcon);

    if (!Database::instance().open()) {
        QMessageBox::critical(nullptr, "错误", "无法打开本地数据库，程序将退出。");
        return 1;
    }

    MainWindow w;
    w.setWindowIcon(appIcon);   // explicitly set on the window (X11 _NET_WM_ICON)
    w.show();

    const int ret = app.exec();

    Database::instance().close();
    return ret;
}
