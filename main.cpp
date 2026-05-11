/**
 * @file main.cpp
 * @brief Application entry point that configures Qt/VTK OpenGL and launches the main window.
 */

// force NVIDIA GPU on hybrid laptops (prevents "wrong device" error in VR)
extern "C" { __declspec(dllexport) unsigned long NvOptimusEnablement = 1; }

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
