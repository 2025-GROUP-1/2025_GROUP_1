// entry point - sets up OpenGL surface format and launches the main window

#include "mainwindow.h"

#include <QApplication>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>

int main(int argc, char* argv[])
{
    // VTK needs this surface format set before any Qt widgets are created
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    QApplication app(argc, argv);

    app.setApplicationName("VRBaseStation");
    app.setOrganizationName("EEEE2076 Group 1");

    MainWindow w;
    w.show();

    return app.exec();
}
