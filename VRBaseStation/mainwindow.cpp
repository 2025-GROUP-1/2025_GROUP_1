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
    // Safety check
    if (!m_partList) {
        emit statusUpdateMessage(tr("Error: No part list loaded."), 2000);
        return;
    }

    // Check if running
    if (m_vrThread && m_vrThread->isRunning()) {
        emit statusUpdateMessage(tr("VR is already running."), 2000);
        return;
    }

    m_vrThread = new VRRenderThread(this);
    m_vrThread->setPartList(m_partList);

    for (ModelPart* p : m_partList->allParts()) {
        m_vrThread->addActorOffline(p->getVRActor(), p->getID());
    }

    // TEST SPHERE: Place it at eye level 1 meter in front
    vtkNew<vtkSphereSource> sphere;
    vtkNew<vtkPolyDataMapper> m;
    m->SetInputConnection(sphere->GetOutputPort());
    vtkNew<vtkActor> a;
    a->SetMapper(m);
    a->SetPosition(0, 1.2, 0); // Directly in front of the new camera position
    m_vrThread->addActorOffline(a, 999);

    m_vrThread->start();

    ui->pushStartVR->setEnabled(false);
    ui->pushStopVR->setEnabled(true);
    emit statusUpdateMessage(tr("VR started."), 2000);
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
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open STL File"),
        "C:\\",
        tr("STL Files (*.stl);;All Files (*.*)")
    );

    if (fileName.isEmpty()) return;

    QModelIndex index = ui->treeView->currentIndex();

    if (!index.isValid()) {
        emit statusUpdateMessage(QString("Please select a tree item first"), 0);
        return;
    }

    ModelPart* selectedPart = static_cast<ModelPart*>(index.internalPointer());
    if (selectedPart == nullptr) return;

    QFileInfo fileInfo(fileName);
    QString partName = fileInfo.fileName();

    selectedPart->setData(0, QVariant(partName));
    selectedPart->loadSTL(fileName);

    emit m_partList->dataChanged(
        m_partList->index(0, 0, QModelIndex()),
        m_partList->index(m_partList->rowCount(QModelIndex()) - 1, 1, QModelIndex())
    );

    updateRender();
    renderer->ResetCamera();
    renderWindow->Render();

    emit statusUpdateMessage(QString("Loaded: ") + partName, 0);
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