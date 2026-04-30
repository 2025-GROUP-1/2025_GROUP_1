/**
 * @file mainwindow.h
 * @brief Top-level window for the application.
 */

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

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @class MainWindow
 * @brief Application main window. Holds parts browser, viewport, properties and theme.
 */
    class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /** @brief Theme variants used by applyTheme(). */
    enum class Theme { Dark, Light };

    /** @brief Construct the window and set up renderer, lighting and theme. */
    MainWindow(QWidget* parent = nullptr);

    /** @brief Destructor. */
    ~MainWindow();

    /** @brief Rebuild the renderer's actor list from the current tree. */
    void updateRender();

    /** @brief Recursive walk used by updateRender(). */
    void updateRenderFromTree(const QModelIndex& index);

    /** @brief Apply a colour theme (dark or light) across the whole window. */
    void applyTheme(Theme theme);

public slots:
    /** @brief Status bar update when a tree item is clicked. */
    void handleTreeClicked();

    /** @brief Refresh side controls when the selected item changes. */
    void onCurrentSelectionChanged(const QModelIndex& current, const QModelIndex& previous);

    // -- File / Tree actions --
    void on_actionImport_Mesh_triggered();
    void on_actionImport_Folder_triggered();
    void on_actionEdit_Part_triggered();
    void on_actionDelete_Part_triggered();

    // -- Camera / Scene --
    void on_actionFrame_All_triggered();
    void on_buttonViewportBackground_clicked();

    // -- Properties: Part --
    void on_buttonDiffuseColour_clicked();
    void on_checkShowPart_stateChanged(int state);
    void on_toggleShrink_toggled(bool checked);
    void on_toggleClip_toggled(bool checked);

    // -- Properties: Scene --
    void on_sliderBrightness_valueChanged(int value);

    // -- Explode view --
    void on_sliderExplode_valueChanged(int value);

    // -- Theme --
    /** @brief Toggle between dark and light themes. */
    void on_actionToggle_Theme_triggered();

    // -- VR --
    void on_actionEnter_VR_triggered();
    void on_actionExit_VR_triggered();
    void on_buttonSyncVR_clicked();

    // -- Help --
    void on_actionAbout_triggered();

signals:
    /**
     * @brief Emitted whenever the status bar should display a new message.
     * @param message Text to display.
     * @param timeout Time in ms before the message is cleared (0 = permanent).
     */
    void statusUpdateMessage(const QString& message, int timeout);

private:
    /** @brief Helper: get the currently selected ModelPart, or nullptr. */
    ModelPart* currentPart();

    /** @brief Recompute scene centre and refresh per-part explode directions. */
    void refreshExplodeDirections();

    /** @brief Recursive helper for refreshExplodeDirections(). */
    void refreshExplodeDirectionsFromTree(const QModelIndex& index, double cx, double cy, double cz);

    /** @brief Apply the current explode amount to every part. */
    void applyExplodeToAll();

    /** @brief Recursive helper for applyExplodeToAll(). */
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