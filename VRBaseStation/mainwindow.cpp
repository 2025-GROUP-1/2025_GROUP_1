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

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_partList = new ModelPartList("Main List", this);
    ui->treeView->setModel(m_partList);

    QModelIndex rootIndex;
    m_partList->appendChild(rootIndex, { "Test Part", "Yes" });

    renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    ui->vtkWidget->setRenderWindow(renderWindow);

    renderer = vtkSmartPointer<vtkRenderer>::New();
    renderWindow->AddRenderer(renderer);

    renderWindow->Render();

    connect(ui->pushButton_2, &QPushButton::released,
        this, &MainWindow::handleButton2);
    connect(this, &MainWindow::statusUpdateMessage,
        ui->statusbar, &QStatusBar::showMessage);
    connect(ui->treeView, &QTreeView::clicked,
        this, &MainWindow::handleTreeClicked);

    ui->treeView->addAction(ui->actionItem_Options);
    this->partList = new ModelPartList("PartsList");
    ui->treeView->setModel(this->partList);

    
    ModelPart* rootItem = this->partList->getRootItem();

    for (int i = 0; i < 3; i++) {
        QString name = QString("TopLevel %1").arg(i);
        QString visible = QString("true");
        ModelPart* childItem = new ModelPart(
            QList<QVariant>({ name, visible }));
        rootItem->appendChild(childItem);

        for (int j = 0; j < 5; j++) {
            QString name = QString("Item %1,%2").arg(i).arg(j);
            QString visible = QString("true");
            ModelPart* childChildItem = new ModelPart(
                QList<QVariant>({ name, visible }));
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

void MainWindow::updateRender()
{
    renderer->RemoveAllViewProps();

    int topLevelCount = partList->rowCount(QModelIndex());
    for (int i = 0; i < topLevelCount; i++) {
        updateRenderFromTree(partList->index(i, 0, QModelIndex()));
    }

    renderWindow->Render();
}

void MainWindow::updateRenderFromTree(const QModelIndex& index)
{
    if (index.isValid()) {
        ModelPart* part = static_cast<ModelPart*>(
            index.internalPointer());
        vtkSmartPointer<vtkActor> actor = part->getActor();
        if (actor != nullptr) {
            renderer->AddActor(actor);
        }
    }

    if (!partList->hasChildren(index) ||
        (index.flags() & Qt::ItemNeverHasChildren)) {
        return;
    }

    int rows = partList->rowCount(index);
    for (int i = 0; i < rows; i++) {
        updateRenderFromTree(partList->index(i, 0, index));
    }
}

void MainWindow::on_pushStartVR_clicked() {
    // Safety check: Don't crash if the list hasn't been created!
    if (!m_partList) {
        emit statusUpdateMessage(tr("Error: No part list loaded."), 2000);
        return;
    }
    // 1. Safety check to see if VR is already running [cite: 417-419]
    if (m_vrThread && m_vrThread->isRunning()) {
        emit statusUpdateMessage(tr("VR is already running."), 2000);
            return;
    }

    // 2. Setup the thread and link it to your parts list [cite: 420-421]
    m_vrThread = new VRRenderThread(this);
        m_vrThread->setPartList(m_partList);

        // 3. Hand every current part's VR actor to the thread before it starts [cite: 422-424]
        for(ModelPart * p : m_partList->allParts()) {
            m_vrThread->addActorOffline(p->getVRActor(), p->getID()); // Added p->getID() 
        }

    // 4. Start the background loop [cite: 425]
    m_vrThread->start();

    // 5. Update UI states [cite: 428-430]
    ui->pushStartVR->setEnabled(false);
        ui->pushStopVR->setEnabled(true);
        emit statusUpdateMessage(tr("VR started."), 2000);
}

void MainWindow::on_pushStopVR_clicked() {
    // 1. Only run if the thread actually exists and is active [cite: 434]
    if (!m_vrThread || !m_vrThread->isRunning()) return;

    // 2. Issue the EndRender command and wait for completion [cite: 435-436]
    m_vrThread->issueCommand(Command::EndRender, 0, QVariant());
        m_vrThread->wait();

        // 3. Clean up the memory [cite: 437-438]
        delete m_vrThread;
        m_vrThread = nullptr;

        // 4. Reset UI states [cite: 439-441]
        ui->pushStartVR->setEnabled(true);
        ui->pushStopVR->setEnabled(false);
        emit statusUpdateMessage(tr("VR stopped."), 2000);
}

void MainWindow::handleButton2()
{
    QModelIndex index = ui->treeView->currentIndex();

    if (!index.isValid()) {
        emit statusUpdateMessage(QString("No item selected"), 0);
        return;
    }

    ModelPart* selectedPart = static_cast<ModelPart*>(
        index.internalPointer());
    if (selectedPart == nullptr) return;

    OptionDialog dialog(this);
    dialog.loadFromModelPart(selectedPart);

    if (dialog.exec() == QDialog::Accepted) {
        dialog.saveToModelPart(selectedPart);

        emit partList->dataChanged(
            partList->index(0, 0, QModelIndex()),
            partList->index(
                partList->rowCount(QModelIndex()) - 1,
                1,
                QModelIndex()));

        updateRender();
        renderWindow->Render();

        emit statusUpdateMessage(
            QString("Item updated: ") +
            selectedPart->data(0).toString(), 0);
    }
    else {
        emit statusUpdateMessage(QString("Edit cancelled"), 0);
    }
}

void MainWindow::handleTreeClicked()
{
    QModelIndex index = ui->treeView->currentIndex();
    if (!index.isValid()) return;

    ModelPart* selectedPart = static_cast<ModelPart*>(
        index.internalPointer());
    if (selectedPart == nullptr) return;

    QString text = selectedPart->data(0).toString();
    emit statusUpdateMessage(
        QString("The selected item is: ") + text, 0);
}

void MainWindow::on_actionOpen_File_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open STL File"),
        "C:\\",
        tr("STL Files (*.stl);;All Files (*.*)")
    );

    if (fileName.isEmpty()) return;

    QModelIndex index = ui->treeView->currentIndex();

    if (!index.isValid()) {
        emit statusUpdateMessage(
            QString("Please select a tree item first"), 0);
        return;
    }

    ModelPart* selectedPart = static_cast<ModelPart*>(
        index.internalPointer());
    if (selectedPart == nullptr) return;

    QFileInfo fileInfo(fileName);
    QString partName = fileInfo.fileName();

    selectedPart->setData(0, QVariant(partName));
    selectedPart->loadSTL(fileName);

    emit partList->dataChanged(
        partList->index(0, 0, QModelIndex()),
        partList->index(
            partList->rowCount(QModelIndex()) - 1,
            1,
            QModelIndex()));

    updateRender();
    renderer->ResetCamera();
    renderWindow->Render();

    emit statusUpdateMessage(QString("Loaded: ") + partName, 0);
}

void MainWindow::on_actionItem_Options_triggered()
{
    QModelIndex index = ui->treeView->currentIndex();

    if (!index.isValid()) {
        emit statusUpdateMessage(QString("No item selected"), 0);
        return;
    }

    ModelPart* selectedPart = static_cast<ModelPart*>(
        index.internalPointer());
    if (selectedPart == nullptr) return;

    OptionDialog dialog(this);
    dialog.loadFromModelPart(selectedPart);

    if (dialog.exec() == QDialog::Accepted) {
        dialog.saveToModelPart(selectedPart);

        emit partList->dataChanged(
            partList->index(0, 0, QModelIndex()),
            partList->index(
                partList->rowCount(QModelIndex()) - 1,
                1,
                QModelIndex()));

        updateRender();
        renderer->ResetCamera();
        renderWindow->Render();

        emit statusUpdateMessage(
            QString("Item Options saved: ") +
            selectedPart->data(0).toString(), 0);
    }
    else {
        emit statusUpdateMessage(
            QString("Item Options cancelled"), 0);
    }
}
