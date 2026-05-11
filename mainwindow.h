/**
 * @file mainwindow.h
 * @brief Declares the main Qt window, CAD tree actions, viewport controls, and VR commands.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModelIndex>
#include <QObject>

#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkLight.h>

#include "ModelPart.h"
#include "ModelPartList.h"
#include "VRRenderThread.h"
#include <vtkSkybox.h>

QT_BEGIN_NAMESPACE
/**
 * @namespace Ui
 * @brief Qt Designer generated widget classes used by the application dialogs and main window.
 */
namespace Ui { class MainWindow; }
QT_END_NAMESPACE
class QEvent;

/**
 * @class MainWindow
 * @brief Main application window that coordinates the part tree, VTK viewport, themes, and VR session.
 */
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
    void on_buttonToggleVisible_clicked(bool checked);
    void on_buttonToggleShrink_clicked(bool checked);
    void on_buttonToggleClip_clicked(bool checked);

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

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool m_passthroughEnabled = false;

    // helpers
    ModelPart* currentPart();
    QList<ModelPart*> selectedParts();
    void refreshPropertyToggleText();

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
    QObject* m_themeToggleContainer = nullptr;
    QObject* m_themeToggleLabel = nullptr;
    QObject* m_themeToggle = nullptr;
};

#endif
