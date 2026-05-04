#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "optiondialog.h"
#include <QFileDialog>
#include <QFileInfo>
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkNew.h>
#include <vtkProperty.h>
#include <vtkCamera.h>
#include <vtkSphereSource.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    _putenv_s("VTK_VR_SIMULATOR", "1");

    ui->setupUi(this);

    // 1. Initialize the main list and give it to the UI
    m_partList = new ModelPartList("Main List", this);
    ui->treeView->setModel(m_partList);

    QModelIndex rootIndex;
    m_partList->appendChild(rootIndex, { "Test Part", "Yes" });

    // 2. Setup VTK rendering
    renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    ui->vtkWidget->setRenderWindow(renderWindow);

    renderer = vtkSmartPointer<vtkRenderer>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->Render();

    // 3. Connect signals
    connect(ui->pushButton_2, &QPushButton::released, this, &MainWindow::handleButton2);
    connect(this, &MainWindow::statusUpdateMessage, ui->statusbar, &QStatusBar::showMessage);
    connect(ui->treeView, &QTreeView::clicked, this, &MainWindow::handleTreeClicked);
    ui->treeView->addAction(ui->actionItem_Options);

    // 4. Populate dummy data for testing
    ModelPart* rootItem = m_partList->getRootItem();

    for (int i = 0; i < 3; i++) {
        QString name = QString("TopLevel %1").arg(i);
        QString visible = QString("true");
        ModelPart* childItem = new ModelPart(QList<QVariant>({ name, visible }));
        rootItem->appendChild(childItem);

        for (int j = 0; j < 5; j++) {
            QString childName = QString("Item %1,%2").arg(i).arg(j);
            ModelPart* childChildItem = new ModelPart(QList<QVariant>({ childName, visible }));
            childItem->appendChild(childChildItem);
        }
    }

    m_vrThread = nullptr;
}

MainWindow::~MainWindow() {
    if (m_vrThread && m_vrThread->isRunning()) {
        m_vrThread->issueCommand(Command::EndRender, 0, QVariant());
        m_vrThread->wait();
        delete m_vrThread;
    }
    delete ui;
}

void MainWindow::updateRender() {
    renderer->RemoveAllViewProps();

    int topLevelCount = m_partList->rowCount(QModelIndex());
    for (int i = 0; i < topLevelCount; i++) {
        updateRenderFromTree(m_partList->index(i, 0, QModelIndex()));
    }

    renderWindow->Render();
}

void MainWindow::updateRenderFromTree(const QModelIndex& index) {
    if (index.isValid()) {
        ModelPart* part = static_cast<ModelPart*>(index.internalPointer());
        vtkSmartPointer<vtkActor> actor = part->getActor();
        if (actor != nullptr) {
            renderer->AddActor(actor);
        }
    }

    if (!m_partList->hasChildren(index) || (index.flags() & Qt::ItemNeverHasChildren)) {
        return;
    }

    int rows = m_partList->rowCount(index);
    for (int i = 0; i < rows; i++) {
        updateRenderFromTree(m_partList->index(i, 0, index));
    }
}

void MainWindow::on_pushStartVR_clicked() {
    m_vrThread = new VRRenderThread(this);
    m_vrThread->setPartList(m_partList);

    if (!m_partList) {
        emit statusUpdateMessage(tr("Error: No part list loaded."), 2000);
        return;
    }

    if (m_vrThread && m_vrThread->isRunning()) {
        emit statusUpdateMessage(tr("VR is already running."), 2000);
        return;
    }

    // --- NEW TREE WALKER CODE ---
    // Create a list to help us crawl the tree, starting at the root
    QList<ModelPart*> itemsToProcess;
    itemsToProcess.append(m_partList->getRootItem());

    // Keep looping until we have checked every single item
    while (!itemsToProcess.isEmpty()) {
        // Grab the next item in the line
        ModelPart* currentPart = itemsToProcess.takeFirst();

        // 1. Add it to VR! (We skip the root item itself because it's just the "Part/Visible" header)
        if (currentPart != m_partList->getRootItem() && currentPart->getVRActor() != nullptr) {
            m_vrThread->addActorOffline(currentPart->getVRActor(), currentPart->getID());
        }

        // 2. Add all of this item's children to the end of the line to be processed
        for (int i = 0; i < currentPart->childCount(); i++) {
            itemsToProcess.append(currentPart->child(i));
        }
    }
    // ----------------------------

    m_vrThread->start();
}

void MainWindow::on_pushStopVR_clicked() {
    if (!m_vrThread || !m_vrThread->isRunning()) return;

    m_vrThread->issueCommand(Command::EndRender, 0, QVariant());
    m_vrThread->wait();

    delete m_vrThread;
    m_vrThread = nullptr;

    ui->pushStartVR->setEnabled(true);
    ui->pushStopVR->setEnabled(false);
    emit statusUpdateMessage(tr("VR stopped."), 2000);
}

void MainWindow::handleButton2() {
    QModelIndex index = ui->treeView->currentIndex();

    if (!index.isValid()) {
        emit statusUpdateMessage(QString("No item selected"), 0);
        return;
    }

    ModelPart* selectedPart = static_cast<ModelPart*>(index.internalPointer());
    if (selectedPart == nullptr) return;

    OptionDialog dialog(this);
    dialog.loadFromModelPart(selectedPart);

    if (dialog.exec() == QDialog::Accepted) {
        dialog.saveToModelPart(selectedPart);

        emit m_partList->dataChanged(
            m_partList->index(0, 0, QModelIndex()),
            m_partList->index(m_partList->rowCount(QModelIndex()) - 1, 1, QModelIndex())
        );

        updateRender();
        renderWindow->Render();

        emit statusUpdateMessage(QString("Item updated: ") + selectedPart->data(0).toString(), 0);
    }
    else {
        emit statusUpdateMessage(QString("Edit cancelled"), 0);
    }
}

void MainWindow::handleTreeClicked() {
    QModelIndex index = ui->treeView->currentIndex();
    if (!index.isValid()) return;

    ModelPart* selectedPart = static_cast<ModelPart*>(index.internalPointer());
    if (selectedPart == nullptr) return;

    QString text = selectedPart->data(0).toString();
    emit statusUpdateMessage(QString("The selected item is: ") + text, 0);
}
void MainWindow::on_actionOpen_File_triggered() {
    // 1. Get the file name from the user
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open STL File"),
        "C:\\",
        tr("STL Files (*.stl);;All Files (*.*)")
    );

    if (fileName.isEmpty()) return;

    // 2. Get the currently selected item in the tree
    QModelIndex index = ui->treeView->currentIndex();
    if (!index.isValid()) {
        emit statusUpdateMessage(QString("Please select a tree item first"), 0);
        return;
    }

    ModelPart* selectedPart = static_cast<ModelPart*>(index.internalPointer());
    if (selectedPart == nullptr) return;

    // 3. Update the part with the new STL data
    QFileInfo fileInfo(fileName);
    selectedPart->setData(0, QVariant(fileInfo.fileName()));
    selectedPart->loadSTL(fileName);

    // 4. Update the 2D view
    updateRender();
    renderer->ResetCamera();
    renderWindow->Render();

    // 5. NEW: If VR is already running, update the VR thread immediately
    if (m_vrThread && m_vrThread->isRunning()) {
        m_vrThread->addActorOffline(selectedPart->getVRActor(), selectedPart->getID());
    }

    emit statusUpdateMessage(QString("Loaded: ") + fileInfo.fileName(), 2000);
}

void MainWindow::on_actionItem_Options_triggered() {
    QModelIndex index = ui->treeView->currentIndex();

    if (!index.isValid()) {
        emit statusUpdateMessage(QString("No item selected"), 0);
        return;
    }

    ModelPart* selectedPart = static_cast<ModelPart*>(index.internalPointer());
    if (selectedPart == nullptr) return;

    OptionDialog dialog(this);
    dialog.loadFromModelPart(selectedPart);

    if (dialog.exec() == QDialog::Accepted) {
        dialog.saveToModelPart(selectedPart);

        emit m_partList->dataChanged(
            m_partList->index(0, 0, QModelIndex()),
            m_partList->index(m_partList->rowCount(QModelIndex()) - 1, 1, QModelIndex())
        );

        updateRender();
        renderer->ResetCamera();
        renderWindow->Render();

        emit statusUpdateMessage(QString("Item Options saved: ") + selectedPart->data(0).toString(), 0);
    }
    else {
        emit statusUpdateMessage(QString("Item Options cancelled"), 0);
    }
}