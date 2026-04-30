#include "mainwindow.h"
#include <stdlib.h>
#include <QApplication>

int main(int argc, char *argv[])
{
	_putenv_s("VTK_VR_SIMULATOR", "1");

    vtkObject::GlobalWarningDisplayOff();

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
