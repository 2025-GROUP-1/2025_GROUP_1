/**
 * @file ModelPart.cpp
 * @brief Implementation of ModelPart, including the VTK filter pipeline.
 */

#include "ModelPart.h"

ModelPart::ModelPart(const QList<QVariant>& data, ModelPart* parent)
    : m_itemData(data)
    , m_parentItem(parent)
    , m_colour(255, 255, 255)
    , m_isVisible(true)
    , m_shrinkEnabled(false)
    , m_shrinkFactor(0.8)
    , m_clipEnabled(false)
    , m_clipPlaneX(0.0)
{
}

ModelPart::~ModelPart() noexcept {
    qDeleteAll(m_childItems);
}

void ModelPart::appendChild(ModelPart* item) {
    item->m_parentItem = this;
    m_childItems.append(item);
}

void ModelPart::removeChild(int row) {
    if (row < 0 || row >= m_childItems.size())
        return;
    delete m_childItems.takeAt(row);
}

ModelPart* ModelPart::child(int row) {
    if (row < 0 || row >= m_childItems.size())
        return nullptr;
    return m_childItems.at(row);
}

int ModelPart::childCount() const { return m_childItems.count(); }
int ModelPart::columnCount() const { return m_itemData.count(); }

QVariant ModelPart::data(int column) const {
    if (column < 0 || column >= m_itemData.size())
        return QVariant();
    if (column == 1)
        return m_isVisible ? QString("true") : QString("false");
    return m_itemData.at(column);
}

void ModelPart::setData(int column, QVariant value) {
    if (column < 0 || column >= m_itemData.size())
        return;
    m_itemData.replace(column, value);
}

ModelPart* ModelPart::parentItem() { return m_parentItem; }

int ModelPart::row() const {
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<ModelPart*>(this));
    return 0;
}

bool ModelPart::loadSTL(QString fileName) {
    file = vtkSmartPointer<vtkSTLReader>::New();
    file->SetFileName(fileName.toStdString().c_str());
    file->Update();

    if (file->GetOutput()->GetNumberOfPoints() == 0) {
        file = nullptr;
        return false;
    }

    rebuildPipeline();
    return true;
}

void ModelPart::rebuildPipeline() {
    if (!file)
        return;

    mapper = vtkSmartPointer<vtkDataSetMapper>::New();

    vtkAlgorithmOutput* source = file->GetOutputPort();

    if (m_shrinkEnabled) {
        shrinkFilter = vtkSmartPointer<vtkShrinkFilter>::New();
        shrinkFilter->SetInputConnection(source);
        shrinkFilter->SetShrinkFactor(m_shrinkFactor);
        shrinkFilter->Update();
        source = shrinkFilter->GetOutputPort();
    }

    if (m_clipEnabled) {
        vtkSmartPointer<vtkPlane> plane = vtkSmartPointer<vtkPlane>::New();
        plane->SetOrigin(m_clipPlaneX, 0.0, 0.0);
        plane->SetNormal(-1.0, 0.0, 0.0);

        clipFilter = vtkSmartPointer<vtkClipDataSet>::New();
        clipFilter->SetInputConnection(source);
        clipFilter->SetClipFunction(plane.Get());
        source = clipFilter->GetOutputPort();
    }

    mapper->SetInputConnection(source);

    if (!actor)
        actor = vtkSmartPointer<vtkActor>::New();

    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(
        m_colour.redF(), m_colour.greenF(), m_colour.blueF());
    actor->SetVisibility(m_isVisible ? 1 : 0);
    actor->SetPosition(m_explodeOffset[0], m_explodeOffset[1], m_explodeOffset[2]);
}

vtkSmartPointer<vtkActor> ModelPart::getActor() { return actor; }

void ModelPart::setColour(const QColor& colour) {
    m_colour = colour;
    if (actor)
        actor->GetProperty()->SetColor(
            m_colour.redF(), m_colour.greenF(), m_colour.blueF());
}

QColor ModelPart::getColour() const { return m_colour; }

void ModelPart::setVisible(bool visible) {
    m_isVisible = visible;
    if (m_itemData.size() > 1)
        m_itemData.replace(1, visible ? QString("true") : QString("false"));
    if (actor)
        actor->SetVisibility(m_isVisible ? 1 : 0);
}

bool ModelPart::getVisible() const { return m_isVisible; }

void ModelPart::setShrinkFilter(bool enabled) {
    m_shrinkEnabled = enabled;
    rebuildPipeline();
}

bool ModelPart::getShrinkEnabled() const { return m_shrinkEnabled; }

void ModelPart::setShrinkFactor(double factor) {
    if (factor < 0.1) factor = 0.1;
    if (factor > 1.0) factor = 1.0;
    m_shrinkFactor = factor;
    if (m_shrinkEnabled)
        rebuildPipeline();
}

double ModelPart::getShrinkFactor() const { return m_shrinkFactor; }

void ModelPart::setClipFilter(bool enabled) {
    m_clipEnabled = enabled;
    rebuildPipeline();
}

bool ModelPart::getClipEnabled() const { return m_clipEnabled; }

void ModelPart::setClipPlaneX(double x) {
    m_clipPlaneX = x;
    if (m_clipEnabled)
        rebuildPipeline();
}

double ModelPart::getClipPlaneX() const { return m_clipPlaneX; }
bool ModelPart::getBounds(double bounds[6]) const {
    if (!file)
        return false;

    file->Update();
    file->GetOutput()->GetBounds(bounds);
    return true;
}

void ModelPart::setExplodeOffset(double x, double y, double z) {
    m_explodeOffset[0] = x;
    m_explodeOffset[1] = y;
    m_explodeOffset[2] = z;

    if (actor)
        actor->SetPosition(x, y, z);
}

void ModelPart::clearExplodeOffset() {
    setExplodeOffset(0.0, 0.0, 0.0);
}
