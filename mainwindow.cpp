/**
 * @file mainwindow.cpp
 * @brief Implementation of MainWindow including theming, toggle switches and explode view.
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

 // ===========================================================================
 // Stylesheets
 // ===========================================================================

static const char* DARK_QSS = R"qss(
* {
    color: #e6e8eb;
    font-family: "Segoe UI", "Inter", sans-serif;
    font-size: 10pt;
}
QMainWindow, QWidget#centralwidget {
    background-color: #1e1f24;
}
QFrame#browserFrame, QFrame#propertiesFrame {
    background-color: #25272d;
    border: 1px solid #34373f;
    border-radius: 8px;
}
QFrame#viewportFrame {
    background-color: #15161a;
    border: 1px solid #34373f;
    border-radius: 8px;
}
QLabel#browserHeading, QLabel#propertiesHeading {
    color: #6f7480;
    font-size: 9pt;
    font-weight: 700;
    letter-spacing: 2px;
    padding-bottom: 4px;
}
QLabel#browserHint {
    color: #6f7480;
    font-style: italic;
    font-size: 9pt;
}
QGroupBox {
    background-color: #2b2d34;
    border: 1px solid #34373f;
    border-radius: 6px;
    margin-top: 14px;
    padding: 10px;
    font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 6px;
    color: #b8bcc6;
}
QPushButton {
    background-color: #3a3d46;
    border: 1px solid #4a4e58;
    border-radius: 5px;
    padding: 6px 12px;
    color: #e6e8eb;
}
QPushButton:hover  { background-color: #454953; border-color: #5b606b; }
QPushButton:pressed{ background-color: #2f3138; }
QPushButton:disabled { color: #6a6d75; background-color: #2b2d34; border-color: #34373f; }
QPushButton#buttonEnterVR {
    background-color: #1aa179; border-color: #1aa179; color: white; font-weight: 600;
}
QPushButton#buttonEnterVR:hover { background-color: #20b88a; }
QPushButton#buttonExitVR {
    background-color: #c54848; border-color: #c54848; color: white; font-weight: 600;
}
QPushButton#buttonExitVR:hover { background-color: #d85757; }
QTreeView {
    background-color: #1e1f24;
    border: 1px solid #34373f;
    border-radius: 4px;
    alternate-background-color: #25272d;
    selection-background-color: #1aa179;
    selection-color: white;
    padding: 2px;
}
QTreeView::item { padding: 4px; }
QHeaderView::section {
    background-color: #2b2d34;
    color: #b8bcc6;
    padding: 6px;
    border: none;
    border-right: 1px solid #34373f;
    font-weight: 600;
}
QSlider::groove:horizontal {
    background: #34373f; height: 4px; border-radius: 2px;
}
QSlider::handle:horizontal {
    background: #1aa179; width: 14px; height: 14px;
    margin: -6px 0; border-radius: 7px;
}
QSlider::handle:horizontal:hover { background: #20b88a; }
QSlider::sub-page:horizontal { background: #1aa179; border-radius: 2px; }

/* Default checkbox (small square tick) */
QCheckBox::indicator {
    width: 18px; height: 18px;
    border: 1px solid #4a4e58;
    border-radius: 4px;
    background-color: #2b2d34;
}
QCheckBox::indicator:hover  { border-color: #1aa179; }
QCheckBox::indicator:checked{
    background-color: #1aa179; border-color: #1aa179;
}

/* iOS-style toggle switches: pill shape, accent fill when on */
QCheckBox#checkShowPart::indicator,
QCheckBox#toggleShrink::indicator,
QCheckBox#toggleClip::indicator {
    width: 44px; height: 22px;
    border-radius: 11px;
    border: 1px solid #4a4e58;
    background-color: #34373f;
}
QCheckBox#checkShowPart::indicator:hover,
QCheckBox#toggleShrink::indicator:hover,
QCheckBox#toggleClip::indicator:hover {
    border-color: #5b606b;
}
QCheckBox#checkShowPart::indicator:checked,
QCheckBox#toggleShrink::indicator:checked,
QCheckBox#toggleClip::indicator:checked {
    background-color: #1aa179;
    border-color: #1aa179;
}

QLineEdit, QSpinBox {
    background-color: #1e1f24;
    border: 1px solid #4a4e58;
    border-radius: 4px;
    padding: 4px 6px;
    selection-background-color: #1aa179;
}
QMenuBar {
    background-color: #25272d;
    border-bottom: 1px solid #34373f;
}
QMenuBar::item:selected { background-color: #3a3d46; }
QMenu {
    background-color: #25272d;
    border: 1px solid #34373f;
    padding: 4px;
}
QMenu::item { padding: 6px 24px; border-radius: 3px; }
QMenu::item:selected { background-color: #1aa179; color: white; }
QToolBar {
    background-color: #25272d;
    border: none;
    spacing: 4px;
    padding: 4px;
}
QToolButton {
    background-color: transparent;
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 4px 8px;
}
QToolButton:hover  { background-color: #3a3d46; border-color: #4a4e58; }
QToolButton:pressed{ background-color: #2f3138; }
QStatusBar {
    background-color: #25272d;
    border-top: 1px solid #34373f;
    color: #b8bcc6;
}
QSplitter::handle { background-color: #34373f; }
QSplitter::handle:hover { background-color: #1aa179; }
)qss";

static const char* LIGHT_QSS = R"qss(
* {
    color: #1e1f24;
    font-family: "Segoe UI", "Inter", sans-serif;
    font-size: 10pt;
}
QMainWindow, QWidget#centralwidget {
    background-color: #f4f5f7;
}
QFrame#browserFrame, QFrame#propertiesFrame {
    background-color: #ffffff;
    border: 1px solid #d8dbe0;
    border-radius: 8px;
}
QFrame#viewportFrame {
    background-color: #ffffff;
    border: 1px solid #d8dbe0;
    border-radius: 8px;
}
QLabel#browserHeading, QLabel#propertiesHeading {
    color: #8a8f99;
    font-size: 9pt;
    font-weight: 700;
    letter-spacing: 2px;
    padding-bottom: 4px;
}
QLabel#browserHint {
    color: #8a8f99;
    font-style: italic;
    font-size: 9pt;
}
QGroupBox {
    background-color: #fafbfc;
    border: 1px solid #d8dbe0;
    border-radius: 6px;
    margin-top: 14px;
    padding: 10px;
    font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 6px;
    color: #4a4e58;
}
QPushButton {
    background-color: #ffffff;
    border: 1px solid #c8ccd2;
    border-radius: 5px;
    padding: 6px 12px;
    color: #1e1f24;
}
QPushButton:hover  { background-color: #f0f2f4; border-color: #1aa179; }
QPushButton:pressed{ background-color: #e6e8eb; }
QPushButton:disabled { color: #b8bcc6; background-color: #f4f5f7; border-color: #e0e3e7; }
QPushButton#buttonEnterVR {
    background-color: #1aa179; border-color: #1aa179; color: white; font-weight: 600;
}
QPushButton#buttonEnterVR:hover { background-color: #20b88a; }
QPushButton#buttonExitVR {
    background-color: #c54848; border-color: #c54848; color: white; font-weight: 600;
}
QPushButton#buttonExitVR:hover { background-color: #d85757; }
QTreeView {
    background-color: #ffffff;
    border: 1px solid #d8dbe0;
    border-radius: 4px;
    alternate-background-color: #f7f8fa;
    selection-background-color: #1aa179;
    selection-color: white;
    padding: 2px;
}
QTreeView::item { padding: 4px; }
QHeaderView::section {
    background-color: #f0f2f4;
    color: #4a4e58;
    padding: 6px;
    border: none;
    border-right: 1px solid #d8dbe0;
    font-weight: 600;
}
QSlider::groove:horizontal {
    background: #d8dbe0; height: 4px; border-radius: 2px;
}
QSlider::handle:horizontal {
    background: #1aa179; width: 14px; height: 14px;
    margin: -6px 0; border-radius: 7px;
}
QSlider::handle:horizontal:hover { background: #20b88a; }
QSlider::sub-page:horizontal { background: #1aa179; border-radius: 2px; }

/* Default checkbox */
QCheckBox::indicator {
    width: 18px; height: 18px;
    border: 1px solid #c8ccd2;
    border-radius: 4px;
    background-color: #ffffff;
}
QCheckBox::indicator:hover  { border-color: #1aa179; }
QCheckBox::indicator:checked{
    background-color: #1aa179; border-color: #1aa179;
}

/* iOS-style toggle switches */
QCheckBox#checkShowPart::indicator,
QCheckBox#toggleShrink::indicator,
QCheckBox#toggleClip::indicator {
    width: 44px; height: 22px;
    border-radius: 11px;
    border: 1px solid #c8ccd2;
    background-color: #e0e3e7;
}
QCheckBox#checkShowPart::indicator:hover,
QCheckBox#toggleShrink::indicator:hover,
QCheckBox#toggleClip::indicator:hover {
    border-color: #b8bcc6;
}
QCheckBox#checkShowPart::indicator:checked,
QCheckBox#toggleShrink::indicator:checked,
QCheckBox#toggleClip::indicator:checked {
    background-color: #1aa179;
    border-color: #1aa179;
}

QLineEdit, QSpinBox {
    background-color: #ffffff;
    border: 1px solid #c8ccd2;
    border-radius: 4px;
    padding: 4px 6px;
    selection-background-color: #1aa179;
}
QMenuBar {
    background-color: #ffffff;
    border-bottom: 1px solid #d8dbe0;
}
QMenuBar::item:selected { background-color: #f0f2f4; }
QMenu {
    background-color: #ffffff;
    border: 1px solid #d8dbe0;
    padding: 4px;
}
QMenu::item { padding: 6px 24px; border-radius: 3px; }
QMenu::item:selected { background-color: #1aa179; color: white; }
QToolBar {
    background-color: #ffffff;
    border: none;
    spacing: 4px;
    padding: 4px;
    border-bottom: 1px solid #d8dbe0;
}
QToolButton {
    background-color: transparent;
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 4px 8px;
}
QToolButton:hover  { background-color: #f0f2f4; border-color: #c8ccd2; }
QToolButton:pressed{ background-color: #e6e8eb; }
QStatusBar {
    background-color: #ffffff;
    border-top: 1px solid #d8dbe0;
    color: #4a4e58;
}
QSplitter::handle { background-color: #e0e3e7; }
QSplitter::handle:hover { background-color: #1aa179; }
)qss";

// ===========================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_theme(Theme::Dark)
    , m_explodeAmount(0.0)
{
    _putenv_s("VTK_VR_SIMULATOR", "1");

    ui->setupUi(this);

    renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    ui->vtkWidget->setRenderWindow(renderWindow);

    renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.08, 0.08, 0.10);
    renderWindow->AddRenderer(renderer);

    light = vtkSmartPointer<vtkLight>::New();
    light->SetLightTypeToHeadlight();
    light->SetIntensity(0.8);
    renderer->AddLight(light);

    renderWindow->Render();

    partList = new ModelPartList("PartsList");
    ui->treeView->setModel(partList);
    ui->treeView->addAction(ui->actionEdit_Part);
    ui->treeView->addAction(ui->actionDelete_Part);

    connect(this, &MainWindow::statusUpdateMessage,
        ui->statusbar, &QStatusBar::showMessage);
    connect(ui->treeView, &QTreeView::clicked,
        this, &MainWindow::handleTreeClicked);
    connect(ui->treeView->selectionModel(), &QItemSelectionModel::currentChanged,
        this, &MainWindow::onCurrentSelectionChanged);

    ui->buttonDiffuseColour->setEnabled(false);
    ui->checkShowPart->setEnabled(false);
    ui->toggleShrink->setEnabled(false);
    ui->toggleClip->setEnabled(false);

    applyTheme(Theme::Dark);

    emit statusUpdateMessage(tr("Ready"), 0);
}

MainWindow::~MainWindow()
{
    if (m_vrThread && m_vrThread->isRunning()) {
        m_vrThread->issueCommand(Command::EndRender, 0, QVariant());
        m_vrThread->wait();
    }
    delete m_vrThread;
    delete ui;
}

ModelPart* MainWindow::currentPart()
{
    QModelIndex idx = ui->treeView->currentIndex();
    if (!idx.isValid())
        return nullptr;
    return static_cast<ModelPart*>(idx.internalPointer());
}

void MainWindow::applyTheme(Theme theme)
{
    m_theme = theme;
    qApp->setStyleSheet(theme == Theme::Dark ? DARK_QSS : LIGHT_QSS);

    if (theme == Theme::Dark)
        renderer->SetBackground(0.08, 0.08, 0.10);
    else
        renderer->SetBackground(0.94, 0.95, 0.97);
    renderWindow->Render();

    ui->actionToggle_Theme->setText(
        theme == Theme::Dark ? tr("Light Mode") : tr("Dark Mode"));
}

void MainWindow::updateRender()
{
    renderer->RemoveAllViewProps();

    int topLevelCount = partList->rowCount(QModelIndex());
    for (int i = 0; i < topLevelCount; i++)
        updateRenderFromTree(partList->index(i, 0, QModelIndex()));

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

    if (!partList->hasChildren(index) || (index.flags() & Qt::ItemNeverHasChildren))
        return;

    int rows = partList->rowCount(index);
    for (int i = 0; i < rows; i++)
        updateRenderFromTree(partList->index(i, 0, index));
}

void MainWindow::handleTreeClicked()
{
    ModelPart* part = currentPart();
    if (!part) return;
    emit statusUpdateMessage(
        tr("Selected: %1").arg(part->data(0).toString()), 0);
}

void MainWindow::onCurrentSelectionChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);

    bool hasSelection = current.isValid();
    ui->buttonDiffuseColour->setEnabled(hasSelection);
    ui->checkShowPart->setEnabled(hasSelection);
    ui->toggleShrink->setEnabled(hasSelection);
    ui->toggleClip->setEnabled(hasSelection);

    if (!hasSelection) return;

    ModelPart* part = static_cast<ModelPart*>(current.internalPointer());
    if (!part) return;

    ui->toggleShrink->blockSignals(true);
    ui->toggleClip->blockSignals(true);
    ui->checkShowPart->blockSignals(true);

    ui->toggleShrink->setChecked(part->getShrinkEnabled());
    ui->toggleClip->setChecked(part->getClipEnabled());
    ui->checkShowPart->setChecked(part->getVisible());

    ui->toggleShrink->blockSignals(false);
    ui->toggleClip->blockSignals(false);
    ui->checkShowPart->blockSignals(false);
}

// ---------------------------------------------------------------------------
// File / Tree
// ---------------------------------------------------------------------------

void MainWindow::on_actionImport_Mesh_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Import Mesh"), QString(),
        tr("STL Files (*.stl);;All Files (*.*)"));
    if (fileName.isEmpty()) return;

    QModelIndex parent = ui->treeView->currentIndex();
    QFileInfo info(fileName);

    QModelIndex newIndex = partList->appendChild(
        parent, { info.fileName(), QString("true") });

    ModelPart* newPart = static_cast<ModelPart*>(newIndex.internalPointer());
    if (!newPart) return;

    if (!newPart->loadSTL(fileName)) {
        partList->removeItem(newIndex);
        emit statusUpdateMessage(tr("Import failed: %1").arg(info.fileName()), 0);
        return;
    }

    ui->treeView->expand(parent);
    refreshExplodeDirections();
    applyExplodeToAll();
    updateRender();
    renderer->ResetCamera();
    renderWindow->Render();

    emit statusUpdateMessage(tr("Imported: %1").arg(info.fileName()), 0);
}

void MainWindow::on_actionImport_Folder_triggered()
{
    QString dirPath = QFileDialog::getExistingDirectory(
        this, tr("Import Folder of Meshes"));
    if (dirPath.isEmpty()) return;

    QDir dir(dirPath);
    QStringList stlFiles = dir.entryList(
        QStringList() << "*.stl", QDir::Files, QDir::Name);

    if (stlFiles.isEmpty()) {
        emit statusUpdateMessage(tr("No STL meshes found in %1").arg(dirPath), 0);
        return;
    }

    QModelIndex parent = ui->treeView->currentIndex();
    int loaded = 0;

    for (const QString& fileName : stlFiles) {
        QString fullPath = dir.absoluteFilePath(fileName);
        QModelIndex newIndex = partList->appendChild(
            parent, { fileName, QString("true") });
        ModelPart* newPart = static_cast<ModelPart*>(newIndex.internalPointer());
        if (!newPart) continue;
        if (newPart->loadSTL(fullPath)) loaded++;
        else partList->removeItem(newIndex);
    }

    ui->treeView->expand(parent);
    refreshExplodeDirections();
    applyExplodeToAll();
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
    if (!part) return;

    OptionDialog dialog(this);
    dialog.loadFromModelPart(part);

    if (dialog.exec() != QDialog::Accepted) {
        emit statusUpdateMessage(tr("Edit cancelled"), 0);
        return;
    }

    dialog.saveToModelPart(part);

    emit partList->dataChanged(
        partList->index(0, 0, QModelIndex()),
        partList->index(partList->rowCount(QModelIndex()) - 1, 1, QModelIndex()));

    onCurrentSelectionChanged(index, QModelIndex());
    updateRender();

    emit statusUpdateMessage(tr("Updated: %1").arg(part->data(0).toString()), 0);
}

void MainWindow::on_actionDelete_Part_triggered()
{
    QModelIndex index = ui->treeView->currentIndex();
    if (!index.isValid()) {
        emit statusUpdateMessage(tr("No part selected"), 0);
        return;
    }
    ModelPart* part = static_cast<ModelPart*>(index.internalPointer());
    if (!part) return;

    QString name = part->data(0).toString();
    partList->removeItem(index);

    refreshExplodeDirections();
    applyExplodeToAll();
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

    QColor chosen = QColorDialog::getColor(current, this, tr("Viewport Background"));
    if (!chosen.isValid()) return;

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
    if (!chosen.isValid()) return;

    part->setColour(chosen);
    renderWindow->Render();
    emit statusUpdateMessage(tr("Recoloured: %1").arg(part->data(0).toString()), 0);
}

void MainWindow::on_checkShowPart_stateChanged(int state)
{
    ModelPart* part = currentPart();
    if (!part) return;
    part->setVisible(state == Qt::Checked);
    updateRender();
}

void MainWindow::on_toggleShrink_toggled(bool checked)
{
    ModelPart* part = currentPart();
    if (!part) return;
    part->setShrinkFilter(checked);
    updateRender();
    emit statusUpdateMessage(
        checked ? tr("Shrink filter on: %1").arg(part->data(0).toString())
        : tr("Shrink filter off: %1").arg(part->data(0).toString()), 0);
}

void MainWindow::on_toggleClip_toggled(bool checked)
{
    ModelPart* part = currentPart();
    if (!part) return;
    part->setClipFilter(checked);
    updateRender();
    emit statusUpdateMessage(
        checked ? tr("Clip filter on: %1").arg(part->data(0).toString())
        : tr("Clip filter off: %1").arg(part->data(0).toString()), 0);
}

// ---------------------------------------------------------------------------
// Properties: Scene
// ---------------------------------------------------------------------------

void MainWindow::on_sliderBrightness_valueChanged(int value)
{
    double intensity = (value / 100.0) * 1.5;
    light->SetIntensity(intensity);
    renderWindow->Render();
}

// ---------------------------------------------------------------------------
// Explode view
// ---------------------------------------------------------------------------

void MainWindow::on_sliderExplode_valueChanged(int value)
{
    m_explodeAmount = value / 100.0;
    applyExplodeToAll();
    renderWindow->Render();
    emit statusUpdateMessage(tr("Explode: %1%").arg(value), 0);
}

void MainWindow::refreshExplodeDirections()
{
    double xSum = 0.0, ySum = 0.0, zSum = 0.0;
    int count = 0;

    int topLevelCount = partList->rowCount(QModelIndex());
    for (int i = 0; i < topLevelCount; i++) {
        QModelIndex idx = partList->index(i, 0, QModelIndex());
        ModelPart* part = static_cast<ModelPart*>(idx.internalPointer());
        if (part && part->getActor()) {
            double bounds[6];
            part->getActor()->GetBounds(bounds);
            xSum += (bounds[0] + bounds[1]) * 0.5;
            ySum += (bounds[2] + bounds[3]) * 0.5;
            zSum += (bounds[4] + bounds[5]) * 0.5;
            count++;
        }
    }

    if (count == 0) return;

    double cx = xSum / count;
    double cy = ySum / count;
    double cz = zSum / count;

    for (int i = 0; i < topLevelCount; i++)
        refreshExplodeDirectionsFromTree(partList->index(i, 0, QModelIndex()), cx, cy, cz);
}

void MainWindow::refreshExplodeDirectionsFromTree(const QModelIndex& index, double cx, double cy, double cz)
{
    if (index.isValid()) {
        ModelPart* part = static_cast<ModelPart*>(index.internalPointer());
        if (part) part->computeExplodeDirection(cx, cy, cz);
    }
    int rows = partList->rowCount(index);
    for (int i = 0; i < rows; i++)
        refreshExplodeDirectionsFromTree(partList->index(i, 0, index), cx, cy, cz);
}

void MainWindow::applyExplodeToAll()
{
    int topLevelCount = partList->rowCount(QModelIndex());
    for (int i = 0; i < topLevelCount; i++)
        applyExplodeFromTree(partList->index(i, 0, QModelIndex()), m_explodeAmount);
}

void MainWindow::applyExplodeFromTree(const QModelIndex& index, double amount)
{
    if (index.isValid()) {
        ModelPart* part = static_cast<ModelPart*>(index.internalPointer());
        if (part) part->applyExplode(amount);
    }
    int rows = partList->rowCount(index);
    for (int i = 0; i < rows; i++)
        applyExplodeFromTree(partList->index(i, 0, index), amount);
}

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------

void MainWindow::on_actionToggle_Theme_triggered()
{
    applyTheme(m_theme == Theme::Dark ? Theme::Light : Theme::Dark);
    emit statusUpdateMessage(
        m_theme == Theme::Dark ? tr("Switched to dark mode") : tr("Switched to light mode"), 0);
}

// ---------------------------------------------------------------------------
// VR
// ---------------------------------------------------------------------------

void MainWindow::on_actionEnter_VR_triggered()
{
    if (m_vrThread && m_vrThread->isRunning()) {
        emit statusUpdateMessage(tr("VR is already running"), 0);
        return;
    }

    m_vrThread = new VRRenderThread(this);
    m_vrThread->setPartList(partList);

    // Walk every part in the tree and hand its VR actor to the thread
    QList<ModelPart*> queue;
    queue.append(partList->getRootItem());
    while (!queue.isEmpty()) {
        ModelPart* part = queue.takeFirst();
        if (part != partList->getRootItem() && part->getVRActor())
            m_vrThread->addActorOffline(part->getVRActor(), part->getID());
        for (int i = 0; i < part->childCount(); ++i)
            queue.append(part->child(i));
    }

    m_vrThread->start();
    emit statusUpdateMessage(tr("VR started"), 0);
}

void MainWindow::on_actionExit_VR_triggered()
{
    if (!m_vrThread || !m_vrThread->isRunning()) {
        emit statusUpdateMessage(tr("VR is not running"), 0);
        return;
    }

    m_vrThread->issueCommand(Command::EndRender, 0, QVariant());
    m_vrThread->wait();
    delete m_vrThread;
    m_vrThread = nullptr;

    emit statusUpdateMessage(tr("VR stopped"), 0);
}

void MainWindow::on_buttonSyncVR_clicked()
{
    if (!m_vrThread || !m_vrThread->isRunning()) {
        emit statusUpdateMessage(tr("Start VR first"), 0);
        return;
    }

    // Push visibility and colour for every loaded part to the running VR thread
    QList<ModelPart*> queue;
    queue.append(partList->getRootItem());
    while (!queue.isEmpty()) {
        ModelPart* part = queue.takeFirst();
        if (part != partList->getRootItem()) {
            int id = part->getID();
            m_vrThread->issueCommand(Command::SetVisible, id, part->getVisible());
            m_vrThread->issueCommand(Command::SetColour,  id, part->getColour());
        }
        for (int i = 0; i < part->childCount(); ++i)
            queue.append(part->child(i));
    }

    emit statusUpdateMessage(tr("Synced to VR"), 0);
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