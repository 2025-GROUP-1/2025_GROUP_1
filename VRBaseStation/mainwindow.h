#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeView>
#include <QFileDialog>
#include <QFileInfo>
#include <QStatusBar>
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkActor.h>
#include "ModelPart.h"
#include "ModelPartList.h"
#include "VRRenderThread.h"
#include "ModelPartList.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    void updateRender();
    void updateRenderFromTree(const QModelIndex& index);

public slots:
    void handleButton2();
    void handleTreeClicked();
    void on_actionOpen_File_triggered();
    void on_actionItem_Options_triggered();

private slots:
    void on_pushStartVR_clicked();
    void on_pushStopVR_clicked();

signals:
    void statusUpdateMessage(const QString& message, int timeout);

private:
    Ui::MainWindow* ui;
    ModelPartList* partList;
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow;
    VRRenderThread* m_vrThread = nullptr;
    ModelPartList* m_partList = nullptr;
};

#endif
