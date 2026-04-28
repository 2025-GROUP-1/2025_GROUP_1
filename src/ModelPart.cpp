#include "ModelPart.h"
#include <vtkSmartPointer.h>
#include <vtkSTLReader.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>

ModelPart::ModelPart(const QList<QVariant>& data, ModelPart* parent)
    : m_itemData(data), m_parentItem(parent)
{
    red       = 255;
    green     = 255;
    blue      = 255;
    isVisible = true;
    shrinkEnabled = false;
    clipEnabled = false;
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

    mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(file->GetOutputPort());

    actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);

    actor->GetProperty()->SetColor(
        red   / 255.0,
        green / 255.0,
        blue  / 255.0
        );

    actor->SetVisibility(isVisible ? 1 : 0);
}

vtkSmartPointer<vtkActor> ModelPart::getActor() {
    return actor;
}

void ModelPart::setColour(int r, int g, int b) {
    red   = r;
    green = g;
    blue  = b;

    if (actor != nullptr) {
        actor->GetProperty()->SetColor(
            red   / 255.0,
            green / 255.0,
            blue  / 255.0
        );
    }
}
int ModelPart::getColourR() { return red;   }
int ModelPart::getColourG() { return green; }
int ModelPart::getColourB() { return blue;  }

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
void ModelPart::setShrinkEnabled(bool enabled)
{
    shrinkEnabled = enabled;
}

bool ModelPart::getShrinkEnabled()
{
    return shrinkEnabled;
}

void ModelPart::setClipEnabled(bool enabled)
{
    clipEnabled = enabled;
}

bool ModelPart::getClipEnabled()
{
    return clipEnabled;
}