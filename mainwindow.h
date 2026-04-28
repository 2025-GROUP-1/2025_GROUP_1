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

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @class MainWindow
 * @brief Application main window. Holds parts browser, viewport and properties.
 */
    class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /** @brief Construct the window and set up renderer and lighting. */
    MainWindow(QWidget* parent = nullptr);

    /** @brief Destructor. */
    ~MainWindow() noexcept;

    /** @brief Rebuild the renderer's actor list from the current tree. */
    void updateRender();

    /** @brief Recursive walk used by updateRender(). */
    void updateRenderFromTree(const QModelIndex& index);

    /** @brief Apply the current explode amount to all loaded parts. */
    void applyExplode();

    /** @brief Recursive helper used by applyExplode(). */
    void applyExplodeFromTree(const QModelIndex& index, const double sceneCentre[3], double amount);

    /** @brief Recursive helper used to calculate global STL bounds for the explode centre. */
    bool collectBoundsFromTree(const QModelIndex& index, double bounds[6]);

public slots:
    /** @brief Status bar update when a tree item is clicked. */
    void handleTreeClicked();

    /** @brief Refresh side controls when the selected item changes. */
    void onCurrentSelectionChanged(const QModelIndex& current, const QModelIndex& previous);

    // -- File / Tree actions --

    /** @brief Import a single STL mesh. */
    void on_actionImport_Mesh_triggered();
    /** @brief Import every STL mesh in a folder. */
    void on_actionImport_Folder_triggered();
    /** @brief Show OptionDialog for the selected part. */
    void on_actionEdit_Part_triggered();
    /** @brief Delete the selected part from the tree. */
    void on_actionDelete_Part_triggered();

    // -- Camera / Scene --

    /** @brief Frame all parts in the view. */
    void on_actionFrame_All_triggered();
    /** @brief Open colour picker for the renderer background. */
    void on_buttonViewportBackground_clicked();

    // -- Properties panel: Part group --

    /** @brief Open colour picker for the selected part. */
    void on_buttonDiffuseColour_clicked();
    /** @brief Visibility checkbox toggled. */
    void on_checkShowPart_stateChanged(int state);
    /** @brief Shrink slider moved. */
    void on_sliderShrink_valueChanged(int value);
    /** @brief Section plane slider moved. */
    void on_sliderSection_valueChanged(int value);

    // -- Properties panel: Scene group --

    /** @brief Ambient brightness slider moved. */
    void on_sliderBrightness_valueChanged(int value);
    /** @brief Enable or disable the exploded CAD view. */
    void on_checkExplode_stateChanged(int state);
    /** @brief Change the global explode amount for all loaded parts. */
    void on_sliderExplode_valueChanged(int value);
    /** @brief Reset the exploded CAD view back to the assembled model. */
    void on_buttonResetExplode_clicked();

    // -- VR (stubs for now, implemented by Senthil later) --

    /** @brief Start VR rendering thread. */
    void on_actionEnter_VR_triggered();
    /** @brief Stop VR rendering thread. */
    void on_actionExit_VR_triggered();
    /** @brief Force the VR thread to refresh from the GUI state. */
    void on_buttonSyncVR_clicked();

    // -- Help --

    /** @brief Show About dialog. */
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

    Ui::MainWindow* ui;            ///< Generated UI.
    ModelPartList* partList;      ///< Tree model.
    vtkSmartPointer<vtkRenderer>                   renderer;      ///< Renderer for the GUI viewport.
    vtkSmartPointer<vtkGenericOpenGLRenderWindow>  renderWindow;  ///< Render window inside vtkWidget.
    vtkSmartPointer<vtkLight>                      light;         ///< Scene light controlled by the slider.
    bool explodeEnabled;                                          ///< True when exploded view is enabled.
    int explodeValue;                                             ///< Explode slider value, 0-100.
};

#endif