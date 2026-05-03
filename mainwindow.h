/**
 * @file mainwindow.h
 * @brief This file defines the top-level MainWindow class, which manages the QTreeView, the GUI's VTK rendering pipeline, and user interactions.
 */

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

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @defgroup gui GUI Components
 * @brief Qt widgets and dialogs that form the application's user interface.
 *
 * Contains the main window and all modal dialogs. These classes run exclusively
 * on the Qt main thread and communicate with the data model via ModelPartList.
 */

/**
 * @addtogroup gui
 * @{
 */

/**
 * @brief Top-level application window.
 *
 * Owns the QTreeView (populated from a ModelPartList), the VTK renderer and
 * render window, and all toolbar/menu actions.  Co-ordinates user interactions:
 * STL file loading, per-part property editing via OptionDialog, and VTK render
 * updates triggered by model changes.
 *
 * On construction a default tree of 3 top-level parts, each with 5 children,
 * is created as a placeholder until the user loads real STL files.
 *
 * @see ModelPartList, ModelPart, OptionDialog
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the main window and wires up all subsystems.
     *
     * Sets up the VTK renderer and render window, builds the default placeholder
     * part tree, and connects all Qt signals and slots.
     *
     * @param parent Optional parent widget; typically \c nullptr for a top-level window.
     */
    MainWindow(QWidget* parent = nullptr);

    /**
     * @brief Destructor. Deletes the generated UI object.
     */
    ~MainWindow();

    /**
     * @brief Clears the renderer and re-adds actors for every part in the tree.
     *
     * Removes all current actors, traverses the entire ModelPartList via
     * updateRenderFromTree(), adds each non-null actor to the renderer, then
     * calls Render() on the render window.
     *
     * @see updateRenderFromTree()
     */
    void updateRender();

    /**
     * @brief Recursively adds actors to the renderer starting from @p index.
     *
     * If the ModelPart at @p index has a non-null VTK actor it is added to the
     * renderer.  The function then recurses into all child indices.
     *
     * @param index Root of the subtree to traverse; pass an invalid index to skip.
     * @see updateRender()
     */
    void updateRenderFromTree(const QModelIndex& index);

public slots:
    /**
     * @brief Emits a status bar message when button 1 is clicked.
     */
    void handleButton1();

    /**
     * @brief Opens OptionDialog for the selected tree item; saves and re-renders on accept.
     *
     * If no valid item is selected, posts "No item selected" to the status bar.
     * On QDialog::Accepted: calls saveToModelPart(), emits dataChanged() across the
     * whole model, calls updateRender(), and re-renders.
     *
     * @see on_actionItem_Options_triggered()
     */
    void handleButton2();

    /**
     * @brief Emits a status bar message naming the currently selected tree item.
     */
    void handleTreeClicked();

    /**
     * @brief Opens a file dialog, loads the chosen STL into the selected tree item.
     *
     * Presents a file-open dialog filtered to STL files.  Sets the selected part's
     * display name to the file's base name, calls ModelPart::loadSTL(), refreshes
     * the tree model, re-renders, and resets the camera.
     */
    void on_actionOpen_File_triggered();

    /**
     * @brief Opens OptionDialog via the context-menu action; saves and re-renders on accept.
     *
     * Functionally identical to handleButton2() but also calls renderer->ResetCamera()
     * after a successful edit.
     *
     * @see handleButton2()
     */
    void on_actionItem_Options_triggered();

signals:
    /**
     * @brief Emitted to display a message in the main window's status bar.
     * @param message Text to display.
     * @param timeout Display duration in milliseconds; 0 means persist until the next message.
     */
    void statusUpdateMessage(const QString& message, int timeout);

private:
    Ui::MainWindow* ui;       ///< Generated UI class that owns all widgets.
    ModelPartList*  partList; ///< Tree model providing data to the QTreeView.

    vtkSmartPointer<vtkRenderer>               renderer;     ///< VTK renderer for the GUI 3D viewport.
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow; ///< VTK render window embedded in the Qt widget.
};

/** @} */ // end gui

#endif
