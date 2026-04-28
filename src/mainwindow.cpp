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
#include "AboutDialog.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    ui->vtkWidget->setRenderWindow(renderWindow);

    renderer = vtkSmartPointer<vtkRenderer>::New();
    renderWindow->AddRenderer(renderer);

    renderWindow->Render();

    connect(ui->pushButton, &QPushButton::released,
        this, &MainWindow::handleButton1);
    connect(ui->pushButton_2, &QPushButton::released,
        this, &MainWindow::handleButton2);
    connect(this, &MainWindow::statusUpdateMessage,
        ui->statusbar, &QStatusBar::showMessage);
    connect(ui->treeView, &QTreeView::clicked,
        this, &MainWindow::handleTreeClicked);
    connect(ui->treeView, &QTreeView::doubleClicked,
        this, &MainWindow::on_actionItem_Options_triggered);

    ui->treeView->addAction(ui->actionItem_Options);
    this->partList = new ModelPartList("PartsList");
    ui->treeView->setModel(this->partList);
}

MainWindow::~MainWindow()
{
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

void MainWindow::handleButton1()
{
    emit statusUpdateMessage(QString("Button 1 was clicked"), 0);
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

    QFileInfo fileInfo(fileName);
    QString partName = fileInfo.fileName();

    QModelIndex parentIndex = ui->treeView->currentIndex();

    QList<QVariant> partData;
    partData << partName << "true";

    QModelIndex newIndex = partList->appendChild(parentIndex, partData);

    if (!newIndex.isValid()) {
        emit statusUpdateMessage("Failed to add part to tree", 0);
        return;
    }

    ModelPart* newPart = static_cast<ModelPart*>(newIndex.internalPointer());

    if (!newPart) {
        emit statusUpdateMessage("Failed to access new part", 0);
        return;
    }

    newPart->loadSTL(fileName);

    ui->treeView->expandAll();

    updateRender();
    renderer->ResetCamera();
    renderWindow->Render();

    emit statusUpdateMessage(QString("Loaded: ") + partName, 0);
}

void MainWindow::on_actionItem_Options_triggered()
{
    QModelIndex index = ui->treeView->currentIndex();

    if (!index.isValid()) {
        emit statusUpdateMessage(QString("No item selected"), 2000);
        return;
    }

    ModelPart* selectedPart = static_cast<ModelPart*>(
        index.internalPointer());

    if (selectedPart == nullptr) {
        emit statusUpdateMessage(QString("Invalid selected item"), 2000);
        return;
    }

    emit statusUpdateMessage(
        QString("Opening options for: ") +
        selectedPart->data(0).toString(), 2000);

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
            QString("Updated: ") +
            selectedPart->data(0).toString(), 2000);
    }
    else {
        emit statusUpdateMessage(
            QString("Options cancelled"), 2000);
    }
}
void MainWindow::on_actionAbout_triggered()
{
    AboutDialog dialog(this);
    dialog.exec();
}