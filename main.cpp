/*
#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
*/
/**
 * @file main.cpp
 * @brief Application entry point for VR Base Station.
 */

#include "mainwindow.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("VR Base Station");
    app.setOrganizationName("EEEE2076 Group 1");

    MainWindow w;
    w.show();
    return app.exec();
}