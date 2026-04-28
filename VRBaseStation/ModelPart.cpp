
#include "ModelPart.h"
#include <vtkSmartPointer.h>
#include <vtkSTLReader.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>

ModelPart::ModelPart(const QList<QVariant>& data, ModelPart* parent)
    : m_itemData(data), m_parentItem(parent)
{
    red = 255;
    green = 255;
    blue = 255;
    isVisible = true;

    // Initialize the variables you already have
    mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);

    // Default scale to avoid the "White Screen of Death"
    actor->SetScale(0.001, 0.001, 0.001);

    // Initialize the variables you already have VR Clone
    vrMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    vrActor = vtkSmartPointer<vtkActor>::New();
    vrActor->SetMapper(vrMapper);

    // Default scale to avoid the "White Screen of Death"
    vrActor->SetScale(0.001, 0.001, 0.001);
}

ModelPart::~ModelPart() {
    qDeleteAll(m_childItems);
}

void ModelPart::appendChild(ModelPart* item) {
    item->m_parentItem = this;
    m_childItems.append(item);
}

ModelPart* ModelPart::child(int row) {
    if (row < 0 || row >= m_childItems.size())
        return nullptr;
    return m_childItems.at(row);
}

int ModelPart::childCount() const {
    return m_childItems.count();
}

int ModelPart::columnCount() const {
    return m_itemData.count();
}

QVariant ModelPart::data(int column) const {
    if (column < 0 || column >= m_itemData.size())
        return QVariant();

    if (column == 1) {
        return isVisible ? QString("true") : QString("false");
    }

    return m_itemData.at(column);
}

void ModelPart::setData(int column, QVariant value) {
    if (column < 0 || column >= m_itemData.size())
        return;
    m_itemData.replace(column, value);
}

ModelPart* ModelPart::parentItem() {
    return m_parentItem;
}

int ModelPart::row() const {
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(
            const_cast<ModelPart*>(this));
    return 0;
}

void ModelPart::loadSTL(QString fileName) {
    file = vtkSmartPointer<vtkSTLReader>::New();
    file->SetFileName(fileName.toStdString().c_str());
    file->Update();

    // Plug data into the 2D UI
    if (mapper) mapper->SetInputConnection(file->GetOutputPort());

    // Plug data into the VR Thread
    if (vrMapper) vrMapper->SetInputConnection(file->GetOutputPort());

    // Update colors and visibility for both
    if (actor) {
        actor->GetProperty()->SetColor(red / 255.0, green / 255.0, blue / 255.0);
        actor->SetVisibility(isVisible ? 1 : 0);
    }
    if (vrActor) {
        vrActor->GetProperty()->SetColor(red / 255.0, green / 255.0, blue / 255.0);
        vrActor->SetVisibility(isVisible ? 1 : 0);
    }
}

vtkSmartPointer<vtkActor> ModelPart::getActor() {
    return actor;
}

void ModelPart::setColour(int r, int g, int b) {
    red = r;
    green = g;
    blue = b;
}

int ModelPart::getColourR() { return red; }
int ModelPart::getColourG() { return green; }
int ModelPart::getColourB() { return blue; }

void ModelPart::setVisible(bool visible) {
    isVisible = visible;
    if (m_itemData.size() > 1) {
        m_itemData.replace(1, visible ? QString("true") : QString("false"));
    }
    if (actor != nullptr) {
        actor->SetVisibility(isVisible ? 1 : 0);
    }
}

bool ModelPart::getVisible() {
    return isVisible;
}
void ModelPart::rebuildVRPipeline() {
    // This is a stub so Senthil's VR thread can call it 
    // when a part property (like colour) changes.
}


int ModelPart::getID() const {
    return 0; // Stub: Hamza will update this later[cite: 19].
}

vtkActor* ModelPart::getVRActor() const {
    return vrActor; // Stub: Required for the VR loop to link[cite: 424].
}