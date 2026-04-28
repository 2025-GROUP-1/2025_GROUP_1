/**
 * @file mainwindow.cpp
 * @brief Implementation of MainWindow's slots, rendering, and panel controls.
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "optiondialog.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QColorDialog>
#include <QMessageBox>
#include <QItemSelectionModel>

#include <vtkRenderer.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkActor.h>
#include <vtkCamera.h>

#include <algorithm>
#include <cmath>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , explodeEnabled(false)
    , explodeValue(0)
{
    ui->setupUi(this);

    // VTK render window inside the Qt widget.
    renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    ui->vtkWidget->setRenderWindow(renderWindow);

    renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.12, 0.13, 0.16);
    renderWindow->AddRenderer(renderer);

    // Scene light controlled by the brightness slider.
    light = vtkSmartPointer<vtkLight>::New();
    light->SetLightTypeToHeadlight();
    light->SetIntensity(0.8);
    renderer->AddLight(light);

    renderWindow->Render();

    // Tree model + view setup.
    partList = new ModelPartList("PartsList");
    ui->treeView->setModel(partList);

    // Right-click context menu on the tree.
    ui->treeView->addAction(ui->actionEdit_Part);
    ui->treeView->addAction(ui->actionDelete_Part);

    // Wire signals.
    connect(this, &MainWindow::statusUpdateMessage,
        ui->statusbar, &QStatusBar::showMessage);
    connect(ui->treeView, &QTreeView::clicked,
        this, &MainWindow::handleTreeClicked);
    connect(ui->treeView->selectionModel(), &QItemSelectionModel::currentChanged,
        this, &MainWindow::onCurrentSelectionChanged);

    // Disable per-item controls until something is selected.
    ui->buttonDiffuseColour->setEnabled(false);
    ui->checkShowPart->setEnabled(false);
    ui->sliderShrink->setEnabled(false);
    ui->sliderSection->setEnabled(false);

    ui->checkExplode->setChecked(false);
    ui->sliderExplode->setEnabled(false);
    ui->sliderExplode->setValue(0);

    emit statusUpdateMessage(tr("Ready"), 0);
}

MainWindow::~MainWindow() noexcept
{
    delete ui;
}

ModelPart* MainWindow::currentPart()
{
    QModelIndex idx = ui->treeView->currentIndex();
    if (!idx.isValid())
        return nullptr;
    return static_cast<ModelPart*>(idx.internalPointer());
}

void MainWindow::updateRender()
{
    renderer->RemoveAllViewProps();

    int topLevelCount = partList->rowCount(QModelIndex());
    for (int i = 0; i < topLevelCount; i++)
        updateRenderFromTree(partList->index(i, 0, QModelIndex()));

    applyExplode();

    // Lights are wiped by RemoveAllViewProps too; re-add ours.
    renderer->AddLight(light);

    renderWindow->Render();
}

void MainWindow::updateRenderFromTree(const QModelIndex& index)
{
    if (index.isValid()) {
        ModelPart* part = static_cast<ModelPart*>(index.internalPointer());
        if (part) {
            vtkSmartPointer<vtkActor> actor = part->getActor();
            if (actor != nullptr)
                renderer->AddActor(actor);
        }
    }

    if (!partList->hasChildren(index) || ((index.flags() & Qt::ItemNeverHasChildren) != 0))
        return;

    int rows = partList->rowCount(index);
    for (int i = 0; i < rows; i++)
        updateRenderFromTree(partList->index(i, 0, index));
}

bool MainWindow::collectBoundsFromTree(const QModelIndex& index, double bounds[6])
{
    bool foundBounds = false;

    if (index.isValid()) {
        ModelPart* part = static_cast<ModelPart*>(index.internalPointer());
        if (part) {
            double partBounds[6];
            if (part->getBounds(partBounds)) {
                if (bounds[0] > bounds[1]) {
                    for (int i = 0; i < 6; i++)
                        bounds[i] = partBounds[i];
                }
                else {
                    bounds[0] = std::min(bounds[0], partBounds[0]);
                    bounds[1] = std::max(bounds[1], partBounds[1]);
                    bounds[2] = std::min(bounds[2], partBounds[2]);
                    bounds[3] = std::max(bounds[3], partBounds[3]);
                    bounds[4] = std::min(bounds[4], partBounds[4]);
                    bounds[5] = std::max(bounds[5], partBounds[5]);
                }
                foundBounds = true;
            }
        }
    }

    if (!partList->hasChildren(index) || ((index.flags() & Qt::ItemNeverHasChildren) != 0))
        return foundBounds;

    int rows = partList->rowCount(index);
    for (int i = 0; i < rows; i++) {
        if (collectBoundsFromTree(partList->index(i, 0, index), bounds))
            foundBounds = true;
    }

    return foundBounds;
}

void MainWindow::applyExplode()
{
    int topLevelCount = partList->rowCount(QModelIndex());
    if (topLevelCount == 0)
        return;

    double sceneBounds[6] = { 1.0, 0.0, 1.0, 0.0, 1.0, 0.0 };
    bool foundBounds = false;

    for (int i = 0; i < topLevelCount; i++) {
        if (collectBoundsFromTree(partList->index(i, 0, QModelIndex()), sceneBounds))
            foundBounds = true;
    }

    if (!foundBounds)
        return;

    double sceneCentre[3] = {
        (sceneBounds[0] + sceneBounds[1]) * 0.5,
        (sceneBounds[2] + sceneBounds[3]) * 0.5,
        (sceneBounds[4] + sceneBounds[5]) * 0.5
    };

    double amount = explodeEnabled ? (static_cast<double>(explodeValue) / 100.0) : 0.0;

    for (int i = 0; i < topLevelCount; i++)
        applyExplodeFromTree(partList->index(i, 0, QModelIndex()), sceneCentre, amount);
}

void MainWindow::applyExplodeFromTree(const QModelIndex& index, const double sceneCentre[3], double amount)
{
    if (index.isValid()) {
        ModelPart* part = static_cast<ModelPart*>(index.internalPointer());
        if (part) {
            double bounds[6];
            if (part->getBounds(bounds)) {
                double partCentre[3] = {
                    (bounds[0] + bounds[1]) * 0.5,
                    (bounds[2] + bounds[3]) * 0.5,
                    (bounds[4] + bounds[5]) * 0.5
                };

                double direction[3] = {
                    partCentre[0] - sceneCentre[0],
                    partCentre[1] - sceneCentre[1],
                    partCentre[2] - sceneCentre[2]
                };

                double length = std::sqrt(
                    direction[0] * direction[0] +
                    direction[1] * direction[1] +
                    direction[2] * direction[2]);

                if (length < 1.0e-9) {
                    direction[0] = static_cast<double>((part->row() % 3) - 1);
                    direction[1] = static_cast<double>(((part->row() + 1) % 3) - 1);
                    direction[2] = static_cast<double>(((part->row() + 2) % 3) - 1);
                }

                part->setExplodeOffset(
                    direction[0] * amount,
                    direction[1] * amount,
                    direction[2] * amount);
            }
        }
    }

    if (!partList->hasChildren(index) || ((index.flags() & Qt::ItemNeverHasChildren) != 0))
        return;

    int rows = partList->rowCount(index);
    for (int i = 0; i < rows; i++)
        applyExplodeFromTree(partList->index(i, 0, index), sceneCentre, amount);
}

void MainWindow::handleTreeClicked()
{
    ModelPart* part = currentPart();
    if (!part)
        return;
    emit statusUpdateMessage(
        tr("Selected: %1").arg(part->data(0).toString()), 0);
}

void MainWindow::onCurrentSelectionChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);

    bool hasSelection = current.isValid();
    ui->buttonDiffuseColour->setEnabled(hasSelection);
    ui->checkShowPart->setEnabled(hasSelection);
    ui->sliderShrink->setEnabled(hasSelection);
    ui->sliderSection->setEnabled(hasSelection);

    if (!hasSelection)
        return;

    ModelPart* part = static_cast<ModelPart*>(current.internalPointer());
    if (!part)
        return;

    // Block signals so updating controls doesn't fire valueChanged slots.
    ui->sliderShrink->blockSignals(true);
    ui->sliderSection->blockSignals(true);
    ui->checkShowPart->blockSignals(true);

    if (part->getShrinkEnabled()) {
        // Map factor [1.0, 0.1] -> slider [0, 100]
        int v = static_cast<int>((1.0 - part->getShrinkFactor()) / 0.9 * 100.0);
        ui->sliderShrink->setValue(v);
    }
    else {
        ui->sliderShrink->setValue(0);
    }

    if (part->getClipEnabled()) {
        // Map clip X [-1.0, 1.0] -> slider [-50, 50]
        int v = static_cast<int>(part->getClipPlaneX() * 50.0);
        ui->sliderSection->setValue(v);
    }
    else {
        ui->sliderSection->setValue(0);
    }

    ui->checkShowPart->setChecked(part->getVisible());

    ui->sliderShrink->blockSignals(false);
    ui->sliderSection->blockSignals(false);
    ui->checkShowPart->blockSignals(false);
}

// ---------------------------------------------------------------------------
// File / Tree actions
// ---------------------------------------------------------------------------

void MainWindow::on_actionImport_Mesh_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Import Mesh"), QString(),
        tr("STL Files (*.stl);;All Files (*.*)"));

    if (fileName.isEmpty())
        return;

    QModelIndex parent = ui->treeView->currentIndex();
    QFileInfo info(fileName);

    QModelIndex newIndex = partList->appendChild(
        parent, { info.fileName(), QString("true") });

    ModelPart* newPart = static_cast<ModelPart*>(newIndex.internalPointer());
    if (!newPart)
        return;

    if (!newPart->loadSTL(fileName)) {
        partList->removeItem(newIndex);
        emit statusUpdateMessage(
            tr("Import failed: %1").arg(info.fileName()), 0);
        return;
    }

    ui->treeView->expand(parent);
    updateRender();
    renderer->ResetCamera();
    renderWindow->Render();

    emit statusUpdateMessage(
        tr("Imported: %1").arg(info.fileName()), 0);
}

void MainWindow::on_actionImport_Folder_triggered()
{
    QString dirPath = QFileDialog::getExistingDirectory(
        this, tr("Import Folder of Meshes"));

    if (dirPath.isEmpty())
        return;

    QDir dir(dirPath);
    QStringList stlFiles = dir.entryList(
        QStringList() << "*.stl", QDir::Files, QDir::Name);

    if (stlFiles.isEmpty()) {
        emit statusUpdateMessage(
            tr("No STL meshes found in %1").arg(dirPath), 0);
        return;
    }

    QModelIndex parent = ui->treeView->currentIndex();
    int loaded = 0;

    for (const QString& fileName : stlFiles) {
        QString fullPath = dir.absoluteFilePath(fileName);

        QModelIndex newIndex = partList->appendChild(
            parent, { fileName, QString("true") });

        ModelPart* newPart = static_cast<ModelPart*>(newIndex.internalPointer());
        if (!newPart)
            continue;

        if (newPart->loadSTL(fullPath))
            loaded++;
        else
            partList->removeItem(newIndex);
    }

    ui->treeView->expand(parent);
    updateRender();
    renderer->ResetCamera();
    renderWindow->Render();

    emit statusUpdateMessage(
        tr("Imported %1 mesh(es) from %2").arg(loaded).arg(dirPath), 0);
}

void MainWindow::on_actionEdit_Part_triggered()
{
    QModelIndex index = ui->treeView->currentIndex();
    if (!index.isValid()) {
        emit statusUpdateMessage(tr("No part selected"), 0);
        return;
    }

    ModelPart* part = static_cast<ModelPart*>(index.internalPointer());
    if (!part)
        return;

    OptionDialog dialog(this);
    dialog.loadFromModelPart(part);

    if (dialog.exec() != QDialog::Accepted) {
        emit statusUpdateMessage(tr("Edit cancelled"), 0);
        return;
    }

    dialog.saveToModelPart(part);

    // Refresh the tree view in case name or visibility text changed.
    emit partList->dataChanged(
        partList->index(0, 0, QModelIndex()),
        partList->index(partList->rowCount(QModelIndex()) - 1, 1, QModelIndex()));

    // Refresh side controls to reflect any changes from the dialog.
    onCurrentSelectionChanged(index, QModelIndex());

    updateRender();

    emit statusUpdateMessage(
        tr("Updated: %1").arg(part->data(0).toString()), 0);
}

void MainWindow::on_actionDelete_Part_triggered()
{
    QModelIndex index = ui->treeView->currentIndex();
    if (!index.isValid()) {
        emit statusUpdateMessage(tr("No part selected"), 0);
        return;
    }

    ModelPart* part = static_cast<ModelPart*>(index.internalPointer());
    if (!part)
        return;

    QString name = part->data(0).toString();
    partList->removeItem(index);

    updateRender();
    emit statusUpdateMessage(tr("Deleted: %1").arg(name), 0);
}

// ---------------------------------------------------------------------------
// Camera / Scene
// ---------------------------------------------------------------------------

void MainWindow::on_actionFrame_All_triggered()
{
    renderer->ResetCamera();
    renderWindow->Render();
    emit statusUpdateMessage(tr("Camera framed to all parts"), 0);
}

void MainWindow::on_buttonViewportBackground_clicked()
{
    double rgb[3];
    renderer->GetBackground(rgb);
    QColor current = QColor::fromRgbF(rgb[0], rgb[1], rgb[2]);

    QColor chosen = QColorDialog::getColor(
        current, this, tr("Viewport Background"));
    if (!chosen.isValid())
        return;

    renderer->SetBackground(chosen.redF(), chosen.greenF(), chosen.blueF());
    renderWindow->Render();
    emit statusUpdateMessage(tr("Background updated"), 0);
}

// ---------------------------------------------------------------------------
// Properties: Part
// ---------------------------------------------------------------------------

void MainWindow::on_buttonDiffuseColour_clicked()
{
    ModelPart* part = currentPart();
    if (!part) {
        emit statusUpdateMessage(tr("No part selected"), 0);
        return;
    }

    QColor chosen = QColorDialog::getColor(
        part->getColour(), this, tr("Diffuse Colour"));
    if (!chosen.isValid())
        return;

    part->setColour(chosen);
    renderWindow->Render();
    emit statusUpdateMessage(
        tr("Recoloured: %1").arg(part->data(0).toString()), 0);
}

void MainWindow::on_checkShowPart_stateChanged(int state)
{
    ModelPart* part = currentPart();
    if (!part)
        return;
    part->setVisible(state == static_cast<int>(Qt::Checked));
    updateRender();
}

void MainWindow::on_sliderShrink_valueChanged(int value)
{
    ModelPart* part = currentPart();
    if (!part)
        return;

    if (value == 0) {
        part->setShrinkFilter(false);
    }
    else {
        // Slider [1, 100] -> factor [0.99, 0.1]
        double factor = 1.0 - (value / 100.0) * 0.9;
        part->setShrinkFactor(factor);
        if (!part->getShrinkEnabled())
            part->setShrinkFilter(true);
    }

    updateRender();
}

void MainWindow::on_sliderSection_valueChanged(int value)
{
    ModelPart* part = currentPart();
    if (!part)
        return;

    if (value == 0) {
        part->setClipFilter(false);
    }
    else {
        // Slider [-50, 50] -> X [-1.0, 1.0]
        double x = value / 50.0;
        part->setClipPlaneX(x);
        if (!part->getClipEnabled())
            part->setClipFilter(true);
    }

    updateRender();
}

// ---------------------------------------------------------------------------
// Properties: Scene
// ---------------------------------------------------------------------------

void MainWindow::on_sliderBrightness_valueChanged(int value)
{
    // Slider 0-100 -> intensity 0.0-1.5.
    double intensity = (value / 100.0) * 1.5;
    light->SetIntensity(intensity);
    renderWindow->Render();
}

// ---------------------------------------------------------------------------
// VR (stubs - real implementation goes in VRRenderThread)
// ---------------------------------------------------------------------------

void MainWindow::on_checkExplode_stateChanged(int state)
{
    explodeEnabled = (state == static_cast<int>(Qt::Checked));
    ui->sliderExplode->setEnabled(explodeEnabled);

    if (!explodeEnabled) {
        explodeValue = 0;
        ui->sliderExplode->blockSignals(true);
        ui->sliderExplode->setValue(0);
        ui->sliderExplode->blockSignals(false);
    }

    applyExplode();
    renderWindow->Render();

    emit statusUpdateMessage(
        explodeEnabled ? tr("Exploded view enabled") : tr("Exploded view disabled"), 0);
}

void MainWindow::on_sliderExplode_valueChanged(int value)
{
    explodeValue = value;

    if (value > 0 && !explodeEnabled) {
        explodeEnabled = true;
        ui->checkExplode->blockSignals(true);
        ui->checkExplode->setChecked(true);
        ui->checkExplode->blockSignals(false);
        ui->sliderExplode->setEnabled(true);
    }

    applyExplode();
    renderWindow->Render();

    emit statusUpdateMessage(tr("Explode amount: %1%").arg(value), 0);
}

void MainWindow::on_buttonResetExplode_clicked()
{
    explodeEnabled = false;
    explodeValue = 0;

    ui->checkExplode->blockSignals(true);
    ui->sliderExplode->blockSignals(true);
    ui->checkExplode->setChecked(false);
    ui->sliderExplode->setValue(0);
    ui->sliderExplode->setEnabled(false);
    ui->checkExplode->blockSignals(false);
    ui->sliderExplode->blockSignals(false);

    applyExplode();
    renderWindow->Render();

    emit statusUpdateMessage(tr("Exploded view reset"), 0);
}

void MainWindow::on_actionEnter_VR_triggered()
{
    emit statusUpdateMessage(tr("VR not yet implemented"), 0);
}

void MainWindow::on_actionExit_VR_triggered()
{
    emit statusUpdateMessage(tr("VR not yet implemented"), 0);
}

void MainWindow::on_buttonSyncVR_clicked()
{
    emit statusUpdateMessage(tr("VR not yet implemented"), 0);
}

// ---------------------------------------------------------------------------
// Help
// ---------------------------------------------------------------------------

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(
        this,
        tr("About"),
        tr("<h3>VR Base Station</h3>"
            "<p>EEEE2076 - Software Development Group Project.</p>"
            "<p>Loads STL CAD files of a Formula Student car and displays "
            "them in a 3D viewport with VR support.</p>"));
}
