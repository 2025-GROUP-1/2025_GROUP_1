// main window - sets up the UI, renderer, themes, and handles all toolbar/menu actions

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "optiondialog.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QColorDialog>
#include <QMessageBox>
#include <QItemSelectionModel>
#include <QProgressDialog>
#include <QFutureWatcher>
#include <QEventLoop>
#include <QtConcurrent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QCheckBox>
#include <QEvent>
#include <QMouseEvent>

#include <vtkSTLReader.h>

#include <vtkRenderer.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkSkybox.h>

// ===========================================================================
// dark theme stylesheet
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
QPushButton#buttonEnablePassthrough {
    background-color: #3976c8; border-color: #3976c8; color: white; font-weight: 600;
}
QPushButton#buttonEnablePassthrough:hover { background-color: #4686db; }
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

/* default checkbox (small square tick) */
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

/* iOS-style toggle switch for theme */
QCheckBox#themeToggle {
    min-width: 44px; max-width: 44px;
    min-height: 22px; max-height: 22px;
    border-radius: 11px;
    border: 1px solid #4a4e58;
    background-color: #34373f;
    padding: 0px;
}
QCheckBox#themeToggle:hover {
    border-color: #5b606b;
}
QCheckBox#themeToggle:checked {
    background-color: #1aa179;
    border-color: #1aa179;
}
QCheckBox#themeToggle::indicator {
    width: 18px; height: 18px;
    border-radius: 9px;
    background-color: #e6e8eb;
    border: none;
}
QCheckBox#themeToggle::indicator:unchecked {
    margin-left: 2px;
}
QCheckBox#themeToggle::indicator:checked {
    margin-left: 24px;
}
/* Toggle push buttons for visible/shrink/clip */
QPushButton#buttonToggleVisible, QPushButton#buttonToggleShrink, QPushButton#buttonToggleClip {
    border-radius: 4px;
    padding: 4px 12px;
    background-color: #34373f;
    border: 1px solid #4a4e58;
}
QPushButton#buttonToggleVisible:checked, QPushButton#buttonToggleShrink:checked, QPushButton#buttonToggleClip:checked {
    background-color: #1aa179;
    border-color: #1aa179;
}
QPushButton#buttonToggleVisible:hover, QPushButton#buttonToggleShrink:hover, QPushButton#buttonToggleClip:hover {
    border-color: #5b606b;
}
QSplitter {
    background: transparent;
    border: none;
}
QSplitter::handle {
    background-color: #34373f;
    image: none;
}
QSplitter::handle:hover { background-color: #1aa179; }

)qss";

// ===========================================================================
// light theme stylesheet
// ===========================================================================

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
QPushButton#buttonEnablePassthrough {
    background-color: #3976c8; border-color: #3976c8; color: white; font-weight: 600;
}
QPushButton#buttonEnablePassthrough:hover { background-color: #4686db; }
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

/* default checkbox */
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

/* iOS-style toggle switch for theme */
QCheckBox#themeToggle {
    min-width: 44px; max-width: 44px;
    min-height: 22px; max-height: 22px;
    border-radius: 11px;
    border: 1px solid #c8ccd2;
    background-color: #e0e3e7;
    padding: 0px;
}
QCheckBox#themeToggle:hover {
    border-color: #b8bcc6;
}
QCheckBox#themeToggle:checked {
    background-color: #1aa179;
    border-color: #1aa179;
}
QCheckBox#themeToggle::indicator {
    width: 18px; height: 18px;
    border-radius: 9px;
    background-color: #ffffff;
    border: none;
}
QCheckBox#themeToggle::indicator:unchecked {
    margin-left: 2px;
}
QCheckBox#themeToggle::indicator:checked {
    margin-left: 24px;
}
/* Toggle push buttons for visible/shrink/clip */
QPushButton#buttonToggleVisible, QPushButton#buttonToggleShrink, QPushButton#buttonToggleClip {
    border-radius: 4px;
    padding: 4px 12px;
    background-color: #e0e3e7;
    border: 1px solid #c8ccd2;
}
QPushButton#buttonToggleVisible:checked, QPushButton#buttonToggleShrink:checked, QPushButton#buttonToggleClip:checked {
    background-color: #1aa179;
    border-color: #1aa179;
    color: white;
}
QPushButton#buttonToggleVisible:hover, QPushButton#buttonToggleShrink:hover, QPushButton#buttonToggleClip:hover {
    border-color: #b8bcc6;
}
QSplitter {
    background: transparent;
    border: none;
}
QSplitter::handle {
    background-color: #e0e3e7;
    image: none;
}
QSplitter::handle:hover { background-color: #1aa179; }
)qss";

// ===========================================================================
// constructor / destructor
// ===========================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_theme(Theme::Dark)
    , m_explodeAmount(0.0)
{
    ui->setupUi(this);
    setAcceptDrops(true);

    // set up VTK renderer and render window inside the Qt widget
    renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    ui->vtkWidget->setRenderWindow(renderWindow);

    renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.08, 0.08, 0.10);
    renderWindow->AddRenderer(renderer);

    // headlight so parts are always lit from the camera direction
    light = vtkSmartPointer<vtkLight>::New();
    light->SetLightTypeToHeadlight();
    light->SetIntensity(0.8);
    renderer->AddLight(light);

    renderWindow->Render();

    // parts tree model
    partList = new ModelPartList("PartsList");
    ui->treeView->setModel(partList);
    ui->treeView->addAction(ui->actionEdit_Part);
    ui->treeView->addAction(ui->actionDelete_Part);

    // wire up signals
    connect(this, &MainWindow::statusUpdateMessage,
        ui->statusbar, &QStatusBar::showMessage);
    connect(ui->treeView, &QTreeView::clicked,
        this, &MainWindow::handleTreeClicked);
    connect(ui->treeView->selectionModel(), &QItemSelectionModel::currentChanged,
        this, &MainWindow::onCurrentSelectionChanged);

    // disable property controls until something is selected
    ui->buttonDiffuseColour->setEnabled(false);
    ui->buttonToggleVisible->setEnabled(false);
    ui->buttonToggleShrink->setEnabled(false);
    ui->buttonToggleClip->setEnabled(false);
    refreshPropertyToggleText();

    applyTheme(Theme::Dark);

    emit statusUpdateMessage(tr("Ready"), 0);

    // move theme toggle + VR buttons out of the toolbar to rearrange them
    ui->toolBar->removeAction(ui->actionToggle_Theme);
    ui->toolBar->removeAction(ui->actionEnter_VR);
    ui->toolBar->removeAction(ui->actionExit_VR);
    ui->toolBar->removeAction(ui->actionEnable_Passthrough);

    // spacer pushes the theme toggle to the far right
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->toolBar->addWidget(spacer);

    // theme toggle container (pill switch + label)
    QWidget* themeContainer = new QWidget(this);
    QHBoxLayout* themeLayout = new QHBoxLayout(themeContainer);
    themeLayout->setContentsMargins(0, 0, 10, 0);
    themeLayout->setSpacing(8);

    QCheckBox* themeToggle = new QCheckBox(themeContainer);
    themeToggle->setObjectName("themeToggle");
    themeToggle->setCursor(Qt::PointingHandCursor);

    QLabel* themeLabel = new QLabel("🌜 Dark Mode", themeContainer);
    themeLabel->setMinimumWidth(100);
    themeLabel->setStyleSheet("background: transparent; border: none; font-weight: 600;");

    themeLayout->addWidget(themeToggle);
    themeLayout->addWidget(themeLabel);
    ui->toolBar->addWidget(themeContainer);

    m_themeToggleContainer = themeContainer;
    m_themeToggleLabel = themeLabel;
    m_themeToggle = themeToggle;
    themeContainer->installEventFilter(this);
    themeLabel->installEventFilter(this);

    connect(themeToggle, &QCheckBox::toggled, this, [this, themeLabel](bool checked) {
        applyTheme(checked ? Theme::Light : Theme::Dark);
        themeLabel->setText(checked ? "🌞 Light Mode" : "🌜 Dark Mode");
        emit statusUpdateMessage(checked ? tr("Switched to light mode") : tr("Switched to dark mode"), 0);
        });
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

// ===========================================================================
// helpers
// ===========================================================================

// returns the part currently highlighted in the tree, or nullptr
ModelPart* MainWindow::currentPart()
{
    QModelIndex idx = ui->treeView->currentIndex();
    if (!idx.isValid())
        return nullptr;
    return static_cast<ModelPart*>(idx.internalPointer());
}

// returns all parts that are selected (for multi-select operations)
QList<ModelPart*> MainWindow::selectedParts()
{
    QList<ModelPart*> parts;
    for (const QModelIndex& idx : ui->treeView->selectionModel()->selectedIndexes()) {
        if (idx.column() != 0) continue;
        ModelPart* part = static_cast<ModelPart*>(idx.internalPointer());
        if (part) parts.append(part);
    }
    return parts;
}

void MainWindow::refreshPropertyToggleText()
{
    ui->buttonToggleVisible->setText(
        ui->buttonToggleVisible->isChecked() ? tr("Visible: On") : tr("Visible: Off"));
    ui->buttonToggleShrink->setText(
        ui->buttonToggleShrink->isChecked() ? tr("Shrink: On") : tr("Shrink: Off"));
    ui->buttonToggleClip->setText(
        ui->buttonToggleClip->isChecked() ? tr("Clip: On") : tr("Clip: Off"));
}

// ===========================================================================
// theme
// ===========================================================================

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
    renderWindow->Render();
}

// ===========================================================================
// tree interaction
// ===========================================================================

void MainWindow::handleTreeClicked()
{
    ModelPart* part = currentPart();
    if (!part) return;
    emit statusUpdateMessage(
        tr("Selected: %1").arg(part->data(0).toString()), 0);
}

// updates the side panel controls when the selected part changes
void MainWindow::onCurrentSelectionChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);

    bool hasSelection = current.isValid();
    ui->buttonDiffuseColour->setEnabled(hasSelection);
    ui->buttonToggleVisible->setEnabled(hasSelection);
    ui->buttonToggleShrink->setEnabled(hasSelection);
    ui->buttonToggleClip->setEnabled(hasSelection);

    if (!hasSelection) {
        refreshPropertyToggleText();
        return;
    }

    ModelPart* part = static_cast<ModelPart*>(current.internalPointer());
    if (!part) return;

    // block signals so we don't accidentally trigger toggle handlers
    ui->buttonToggleShrink->blockSignals(true);
    ui->buttonToggleClip->blockSignals(true);
    ui->buttonToggleVisible->blockSignals(true);

    ui->buttonToggleShrink->setChecked(part->getShrinkEnabled());
    ui->buttonToggleClip->setChecked(part->getClipEnabled());
    ui->buttonToggleVisible->setChecked(part->getVisible());

    ui->buttonToggleShrink->blockSignals(false);
    ui->buttonToggleClip->blockSignals(false);
    ui->buttonToggleVisible->blockSignals(false);
    refreshPropertyToggleText();
}

// ===========================================================================
// file / tree actions
// ===========================================================================

void MainWindow::on_buttonSelectAll_clicked()
{
    ui->treeView->selectAll();
    emit statusUpdateMessage(tr("All parts selected"), 0);
}

// imports a single STL file
void MainWindow::on_actionImport_Mesh_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Import Mesh"), QString(),
        tr("STL Files (*.stl);;All Files (*.*)"));
    if (fileName.isEmpty()) return;

    QModelIndex parent = ui->treeView->currentIndex();
    QFileInfo info(fileName);

    QModelIndex newIndex = partList->appendChild(parent, { info.fileName(), QString("true") });
    ModelPart* newPart = static_cast<ModelPart*>(newIndex.internalPointer());
    if (!newPart) return;

    if (!newPart->loadSTL(fileName)) {
        partList->removeItem(newIndex);
        emit statusUpdateMessage(tr("Import failed: %1").arg(info.fileName()), 0);
        return;
    }

    renderer->AddActor(newPart->getActor());
    ui->treeView->expand(parent);
    ui->treeView->setCurrentIndex(newIndex);
    refreshExplodeDirections();
    applyExplodeToAll();
    renderer->ResetCamera();
    renderWindow->Render();

    // push to VR if the thread is already running
    if (m_vrThread && m_vrThread->isRunning()) {
        newPart->rebuildVRPipeline();
        if (newPart->getVRActor())
            m_vrThread->issueCommand(Command::AddActor, newPart->getID(), newPart->getVRActor());
    }

    emit statusUpdateMessage(tr("Imported: %1").arg(info.fileName()), 0);
}

// imports all STL files from a folder (parallel file I/O)
void MainWindow::on_actionImport_Folder_triggered()
{
    QString dirPath = QFileDialog::getExistingDirectory(this, tr("Import Folder of Meshes"));
    if (dirPath.isEmpty()) return;

    QDir dir(dirPath);
    QStringList fileNames = dir.entryList(QStringList() << "*.stl", QDir::Files, QDir::Name);
    if (fileNames.isEmpty()) {
        emit statusUpdateMessage(tr("No STL files found in %1").arg(dirPath), 0);
        return;
    }

    // one reader per file, Update() runs in parallel for speed
    struct LoadTask { QString fullPath; QString fileName; vtkSmartPointer<vtkSTLReader> reader; };
    QVector<LoadTask> tasks;
    tasks.reserve(fileNames.size());
    for (const QString& f : fileNames)
        tasks.append({ dir.absoluteFilePath(f), f, nullptr });

    // create readers on the main thread (VTK factory isn't thread-safe)
    for (LoadTask& t : tasks) {
        t.reader = vtkSmartPointer<vtkSTLReader>::New();
        t.reader->SetFileName(t.fullPath.toStdString().c_str());
    }

    // read files in parallel with a progress bar
    QProgressDialog progress(tr("Reading %1 STL files...").arg(tasks.size()),
        tr("Cancel"), 0, tasks.size(), this);
    progress.setWindowModality(Qt::WindowModal);

    QFutureWatcher<void> watcher;
    connect(&watcher, &QFutureWatcher<void>::progressValueChanged,
        &progress, &QProgressDialog::setValue);
    connect(&progress, &QProgressDialog::canceled, &watcher, &QFutureWatcher<void>::cancel);

    watcher.setFuture(QtConcurrent::map(tasks, [](LoadTask& t) {
        t.reader->Update();
        }));

    QEventLoop loop;
    connect(&watcher, &QFutureWatcher<void>::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (watcher.isCanceled()) {
        emit statusUpdateMessage(tr("Import cancelled"), 0);
        return;
    }

    // build VTK pipelines and add to tree on the main thread
    QModelIndex parent = ui->treeView->currentIndex();
    ui->treeView->setUpdatesEnabled(false);
    int loaded = 0;
    QModelIndex firstLoadedIndex;

    for (const LoadTask& t : tasks) {
        QModelIndex newIndex = partList->appendChild(parent, { t.fileName, QString("true") });
        ModelPart* newPart = static_cast<ModelPart*>(newIndex.internalPointer());
        if (!newPart) continue;

        if (newPart->attachReader(t.reader)) {
            renderer->AddActor(newPart->getActor());
            if (!firstLoadedIndex.isValid())
                firstLoadedIndex = newIndex;
            loaded++;
        }
        else {
            partList->removeItem(newIndex);
        }
    }

    ui->treeView->setUpdatesEnabled(true);
    ui->treeView->expand(parent);
    if (firstLoadedIndex.isValid())
        ui->treeView->setCurrentIndex(firstLoadedIndex);
    refreshExplodeDirections();
    applyExplodeToAll();
    renderer->ResetCamera();
    renderWindow->Render();

    // push all newly loaded parts to VR if the thread is running
    if (m_vrThread && m_vrThread->isRunning()) {
        QList<ModelPart*> bfs;
        bfs.append(partList->getRootItem());
        while (!bfs.isEmpty()) {
            ModelPart* p = bfs.takeFirst();
            if (p != partList->getRootItem() && p->getActor() && !p->getVRActor()) {
                p->rebuildVRPipeline();
                if (p->getVRActor())
                    m_vrThread->issueCommand(Command::AddActor, p->getID(), p->getVRActor());
            }
            for (int i = 0; i < p->childCount(); ++i)
                bfs.append(p->child(i));
        }
    }

    emit statusUpdateMessage(tr("Imported %1/%2 mesh(es) from %3")
        .arg(loaded).arg(tasks.size()).arg(dirPath), 0);
}

// opens the edit dialog for the selected part
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

    // push edited properties to VR
    if (m_vrThread && m_vrThread->isRunning()) {
        int id = part->getID();
        m_vrThread->issueCommand(Command::SetColour,   id, part->getColour());
        m_vrThread->issueCommand(Command::SetVisible,   id, part->getVisible());
        m_vrThread->issueCommand(Command::ToggleShrink, id, part->getShrinkEnabled());
        m_vrThread->issueCommand(Command::ToggleClip,   id, part->getClipEnabled());
    }

    emit partList->dataChanged(
        partList->index(0, 0, QModelIndex()),
        partList->index(partList->rowCount(QModelIndex()) - 1, 1, QModelIndex()));

    onCurrentSelectionChanged(index, QModelIndex());
    updateRender();

    emit statusUpdateMessage(tr("Updated: %1").arg(part->data(0).toString()), 0);
}

// removes the selected part from the tree and renderer
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

    // remove from VR first (before the part is destroyed)
    if (m_vrThread && m_vrThread->isRunning())
        m_vrThread->issueCommand(Command::RemoveActor, part->getID(), part->getVRActor());

    if (part->getActor())
        renderer->RemoveActor(part->getActor());
    partList->removeItem(index);

    refreshExplodeDirections();
    applyExplodeToAll();
    renderWindow->Render();

    emit statusUpdateMessage(tr("Deleted: %1").arg(name), 0);
}

// ===========================================================================
// camera / scene
// ===========================================================================

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

// ===========================================================================
// part properties
// ===========================================================================

void MainWindow::on_buttonDiffuseColour_clicked()
{
    QList<ModelPart*> parts = selectedParts();
    if (parts.isEmpty()) {
        emit statusUpdateMessage(tr("No part selected"), 0);
        return;
    }
    QColor seed = parts.first()->getColour();
    QColor chosen = QColorDialog::getColor(seed, this, tr("Diffuse Colour"));
    if (!chosen.isValid()) return;

    for (ModelPart* part : parts) {
        part->setColour(chosen);
        if (m_vrThread && m_vrThread->isRunning())
            m_vrThread->issueCommand(Command::SetColour, part->getID(), chosen);
    }
    renderWindow->Render();
    emit statusUpdateMessage(
        parts.size() == 1
            ? tr("Recoloured: %1").arg(parts.first()->data(0).toString())
            : tr("Recoloured %1 parts").arg(parts.size()), 0);
}

void MainWindow::on_buttonToggleVisible_clicked(bool checked)
{
    QList<ModelPart*> parts = selectedParts();
    for (ModelPart* part : parts) {
        part->setVisible(checked);
        if (m_vrThread && m_vrThread->isRunning())
            m_vrThread->issueCommand(Command::SetVisible, part->getID(), checked);
    }
    ui->buttonToggleVisible->setChecked(checked);
    refreshPropertyToggleText();
    updateRender();
}

void MainWindow::on_buttonToggleShrink_clicked(bool checked)
{
    QList<ModelPart*> parts = selectedParts();
    for (ModelPart* part : parts) {
        part->setShrinkFilter(checked);
        if (m_vrThread && m_vrThread->isRunning())
            m_vrThread->issueCommand(Command::ToggleShrink, part->getID(), checked);
    }
    ui->buttonToggleShrink->setChecked(checked);
    refreshPropertyToggleText();
    updateRender();
    emit statusUpdateMessage(
        parts.size() == 1
            ? (checked ? tr("Shrink filter on: %1") : tr("Shrink filter off: %1")).arg(parts.first()->data(0).toString())
            : tr("Shrink filter %1 on %2 parts").arg(checked ? tr("on") : tr("off")).arg(parts.size()), 0);
}

void MainWindow::on_buttonToggleClip_clicked(bool checked)
{
    QList<ModelPart*> parts = selectedParts();
    for (ModelPart* part : parts) {
        part->setClipFilter(checked);
        if (m_vrThread && m_vrThread->isRunning())
            m_vrThread->issueCommand(Command::ToggleClip, part->getID(), checked);
    }
    ui->buttonToggleClip->setChecked(checked);
    refreshPropertyToggleText();
    updateRender();
    emit statusUpdateMessage(
        parts.size() == 1
            ? (checked ? tr("Clip filter on: %1") : tr("Clip filter off: %1")).arg(parts.first()->data(0).toString())
            : tr("Clip filter %1 on %2 parts").arg(checked ? tr("on") : tr("off")).arg(parts.size()), 0);
}

// ===========================================================================
// scene properties
// ===========================================================================

void MainWindow::on_sliderBrightness_valueChanged(int value)
{
    double intensity = (value / 100.0) * 1.5;
    light->SetIntensity(intensity);
    renderWindow->Render();
}

// ===========================================================================
// explode view
// ===========================================================================

void MainWindow::on_sliderExplode_valueChanged(int value)
{
    m_explodeAmount = value / 100.0;
    applyExplodeToAll();
    renderWindow->Render();
    emit statusUpdateMessage(tr("Explode: %1%").arg(value), 0);
}

// computes the average centre of all parts, then sets each part's explode direction
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

// recursive helper - sets explode direction for a part and all its children
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

// applies the current slider amount to every part in the tree
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

// ===========================================================================
// theme toggle (toolbar action fallback)
// ===========================================================================

void MainWindow::on_actionToggle_Theme_triggered()
{
    QCheckBox* toggle = this->findChild<QCheckBox*>("themeToggle");
    if (toggle) {
        toggle->setChecked(!toggle->isChecked());
    }
}

// ===========================================================================
// VR
// ===========================================================================

// spins up the VR render thread and sends all loaded parts to it
void MainWindow::on_actionEnter_VR_triggered()
{
    if (m_vrThread && m_vrThread->isRunning()) {
        emit statusUpdateMessage(tr("VR is already running"), 0);
        return;
    }

    m_vrThread = new VRRenderThread(this);
    m_vrThread->setPartList(partList);

    // walk the tree, build VR pipelines, and register each actor
    QList<ModelPart*> queue;
    queue.append(partList->getRootItem());
    while (!queue.isEmpty()) {
        ModelPart* part = queue.takeFirst();
        if (part != partList->getRootItem()) {
            part->rebuildVRPipeline();
            if (part->getVRActor())
                m_vrThread->addActorOffline(part->getVRActor(), part->getID());
        }
        for (int i = 0; i < part->childCount(); ++i)
            queue.append(part->child(i));
    }

    // connect VR menu signals so in-headset changes update the GUI
    connect(m_vrThread, &VRRenderThread::partColourChanged, this, [this](int partID, QColor c) {
        ModelPart* part = partList->findByID(partID);
        if (part && part->getActor()) {
            part->getActor()->GetProperty()->SetColor(c.redF(), c.greenF(), c.blueF());
            renderWindow->Render();
        }
    });
    connect(m_vrThread, &VRRenderThread::partShrinkChanged, this, [this](int partID, bool enabled) {
        ModelPart* part = partList->findByID(partID);
        if (part) {
            part->setShrinkStateOnly(enabled);
            if (currentPart() == part) {
                ui->buttonToggleShrink->blockSignals(true);
                ui->buttonToggleShrink->setChecked(enabled);
                ui->buttonToggleShrink->blockSignals(false);
                refreshPropertyToggleText();
            }
        }
    });
    connect(m_vrThread, &VRRenderThread::partClipChanged, this, [this](int partID, bool enabled) {
        ModelPart* part = partList->findByID(partID);
        if (part) {
            part->setClipStateOnly(enabled);
            if (currentPart() == part) {
                ui->buttonToggleClip->blockSignals(true);
                ui->buttonToggleClip->setChecked(enabled);
                ui->buttonToggleClip->blockSignals(false);
                refreshPropertyToggleText();
            }
        }
    });
    connect(m_vrThread, &VRRenderThread::partVisibilityChanged, this, [this](int partID, bool visible) {
        ModelPart* part = partList->findByID(partID);
        if (part) { part->setVisible(visible); renderWindow->Render(); }
    });

    m_vrThread->start();
    emit statusUpdateMessage(tr("VR started"), 0);
}

// stops the VR thread and cleans up
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

// toggles the skybox on/off for AR passthrough mode
void MainWindow::on_actionEnable_Passthrough_triggered()
{
    m_passthroughEnabled = !m_passthroughEnabled;

    if (m_vrThread && m_vrThread->isRunning()) {
        m_vrThread->issueCommand(Command::ToggleSkybox, 0, !m_passthroughEnabled);

        // re-sync part visibility after skybox change
        QList<ModelPart*> bfsQueue;
        bfsQueue.append(partList->getRootItem());
        while (!bfsQueue.isEmpty()) {
            ModelPart* part = bfsQueue.takeFirst();
            if (part != partList->getRootItem())
                m_vrThread->issueCommand(Command::SetVisible, part->getID(), part->getVisible());
            for (int i = 0; i < part->childCount(); ++i)
                bfsQueue.append(part->child(i));
        }
    }

    const QString label = m_passthroughEnabled ? tr("Disable Passthrough") : tr("Enable Passthrough");
    ui->actionEnable_Passthrough->setText(label);
    ui->buttonEnablePassthrough->setText(label);

    emit statusUpdateMessage(
        m_passthroughEnabled
            ? tr("Skybox hidden — double-press headset system button to see your room")
            : tr("Passthrough disabled — skybox restored"), 0);
}

// pushes current GUI state (colour, visibility) to the VR thread
void MainWindow::on_buttonSyncVR_clicked()
{
    if (!m_vrThread || !m_vrThread->isRunning()) {
        emit statusUpdateMessage(tr("Start VR first"), 0);
        return;
    }

    // full sync: add any parts that are missing in VR, update colour/visibility for all
    QList<ModelPart*> queue;
    queue.append(partList->getRootItem());
    while (!queue.isEmpty()) {
        ModelPart* part = queue.takeFirst();
        if (part != partList->getRootItem()) {
            int id = part->getID();

            // if this part doesn't have a VR actor yet, build and push it
            if (!part->getVRActor() && part->getActor()) {
                part->rebuildVRPipeline();
                if (part->getVRActor())
                    m_vrThread->issueCommand(Command::AddActor, id, part->getVRActor());
            }

            m_vrThread->issueCommand(Command::SetVisible, id, part->getVisible());
            m_vrThread->issueCommand(Command::SetColour, id, part->getColour());
        }
        for (int i = 0; i < part->childCount(); ++i)
            queue.append(part->child(i));
    }

    emit statusUpdateMessage(tr("Synced to VR"), 0);
}

// ===========================================================================
// help
// ===========================================================================

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

// ===========================================================================
// drag and drop STL files
// ===========================================================================

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.toLocalFile().toLower().endsWith(".stl")) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    int imported = 0;
    QModelIndex firstImportedIndex;

    for (const QUrl& url : urls) {
        QString filePath = url.toLocalFile();
        if (!filePath.toLower().endsWith(".stl"))
            continue;

        QFileInfo info(filePath);
        QModelIndex parent = ui->treeView->currentIndex();

        QModelIndex newIndex = partList->appendChild(parent, { info.fileName(), QString("true") });
        ModelPart* newPart = static_cast<ModelPart*>(newIndex.internalPointer());
        if (!newPart) continue;

        if (!newPart->loadSTL(filePath)) {
            partList->removeItem(newIndex);
            continue;
        }

        renderer->AddActor(newPart->getActor());
        ui->treeView->expand(parent);
        if (!firstImportedIndex.isValid())
            firstImportedIndex = newIndex;

        // push to VR if running
        if (m_vrThread && m_vrThread->isRunning()) {
            newPart->rebuildVRPipeline();
            if (newPart->getVRActor())
                m_vrThread->issueCommand(Command::AddActor, newPart->getID(), newPart->getVRActor());
        }

        imported++;
    }

    if (imported > 0) {
        if (firstImportedIndex.isValid())
            ui->treeView->setCurrentIndex(firstImportedIndex);
        refreshExplodeDirections();
        applyExplodeToAll();
        renderer->ResetCamera();
        renderWindow->Render();
        emit statusUpdateMessage(tr("Dropped %1 STL file(s)").arg(imported), 0);
    }

    event->acceptProposedAction();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_themeToggleContainer || watched == m_themeToggleLabel) &&
        event->type() == QEvent::MouseButtonRelease) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (auto* toggle = qobject_cast<QCheckBox*>(m_themeToggle))
                toggle->setChecked(!toggle->isChecked());
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}
