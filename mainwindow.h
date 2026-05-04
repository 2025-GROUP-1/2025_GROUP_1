// main application window - parts browser, 3D viewport, properties panel, VR controls

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModelIndex>

#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkLight.h>

#include "ModelPart.h"
#include "ModelPartList.h"
#include "VRRenderThread.h"
#include <vtkSkybox.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum class Theme { Dark, Light };

    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void updateRender();
    void applyTheme(Theme theme);

public slots:
    // tree interaction
    void handleTreeClicked();
    void onCurrentSelectionChanged(const QModelIndex& current, const QModelIndex& previous);

    // file / tree actions
    void on_buttonSelectAll_clicked();
    void on_actionImport_Mesh_triggered();
    void on_actionImport_Folder_triggered();
    void on_actionEdit_Part_triggered();
    void on_actionDelete_Part_triggered();

    // camera / scene
    void on_actionFrame_All_triggered();
    void on_buttonViewportBackground_clicked();

    // part properties
    void on_buttonDiffuseColour_clicked();
    void on_checkShowPart_stateChanged(int state);
    void on_toggleShrink_toggled(bool checked);
    void on_toggleClip_toggled(bool checked);

    // scene properties
    void on_sliderBrightness_valueChanged(int value);

    // explode view
    void on_sliderExplode_valueChanged(int value);

    // theme toggle
    void on_actionToggle_Theme_triggered();

    // VR
    void on_actionEnter_VR_triggered();
    void on_actionExit_VR_triggered();
    void on_actionEnable_Passthrough_triggered();
    void on_buttonSyncVR_clicked();

    // help
    void on_actionAbout_triggered();

signals:
    void statusUpdateMessage(const QString& message, int timeout);

private:
    bool m_passthroughEnabled = false;

    // helpers
    ModelPart* currentPart();
    QList<ModelPart*> selectedParts();

    // explode view internals
    void refreshExplodeDirections();
    void refreshExplodeDirectionsFromTree(const QModelIndex& index, double cx, double cy, double cz);
    void applyExplodeToAll();
    void applyExplodeFromTree(const QModelIndex& index, double amount);

    Ui::MainWindow* ui;
    ModelPartList* partList;
    vtkSmartPointer<vtkRenderer>                   renderer;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow>  renderWindow;
    vtkSmartPointer<vtkLight>                      light;

    VRRenderThread* m_vrThread = nullptr;

    Theme  m_theme;
    double m_explodeAmount;
};

#endif
