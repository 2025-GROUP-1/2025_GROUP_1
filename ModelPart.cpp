/**
 * @file ModelPart.cpp
 * @brief Implementation of ModelPart, including the VTK filter pipeline and explode view.
 */

#include "ModelPart.h"
#include <cmath>

ModelPart::ModelPart(const QList<QVariant>& data, ModelPart* parent)
    : m_itemData(data)
    , m_parentItem(parent)
    , m_colour(255, 255, 255)
    , m_isVisible(true)
    , m_shrinkEnabled(false)
    , m_shrinkFactor(0.8)
    , m_clipEnabled(false)
    , m_clipPlaneX(0.0)
    , m_originalCentre(0.0f, 0.0f, 0.0f)
    , m_explodeDir(0.0f, 0.0f, 0.0f)
{
}

ModelPart::~ModelPart() {
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
    rebuildVRPipeline();

    if (actor) {
        double bounds[6];
        actor->GetBounds(bounds);
        m_originalCentre = QVector3D(
            static_cast<float>((bounds[0] + bounds[1]) * 0.5),
            static_cast<float>((bounds[2] + bounds[3]) * 0.5),
            static_cast<float>((bounds[4] + bounds[5]) * 0.5));
    }

    return true;
}

void ModelPart::rebuildVRPipeline() {
    if (!file)
        return;

    vrMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    vrMapper->SetInputConnection(file->GetOutputPort());

    if (!vrActor)
        vrActor = vtkSmartPointer<vtkActor>::New();

    vrActor->SetMapper(vrMapper);
    vrActor->SetScale(0.001, 0.001, 0.001);  // STL units are mm; VR space is metres
    vrActor->GetProperty()->SetColor(
        m_colour.redF(), m_colour.greenF(), m_colour.blueF());
    vrActor->SetVisibility(m_isVisible ? 1 : 0);
}

vtkActor* ModelPart::getVRActor() const {
    return vrActor;
}

int ModelPart::getID() const {
    return reinterpret_cast<quintptr>(this) & 0x7fffffff;
}

void ModelPart::rebuildPipeline() {
    if (!file)
        return;

    mapper = vtkSmartPointer<vtkDataSetMapper>::New();

    vtkAlgorithmOutput* source = file->GetOutputPort();

    // Apply clip first (it operates on PolyData from the STL reader directly).
    if (m_clipEnabled) {
        // Compute clip plane position relative to the STL bounds so the
        // slider range maps to the actual model.
        double bounds[6];
        file->GetOutput()->GetBounds(bounds);
        double cx = (bounds[0] + bounds[1]) * 0.5;
        double xMin = bounds[0];
        double xMax = bounds[1];
        // m_clipPlaneX is normalised (-1..1); map it to model space.
        double planeX = cx + m_clipPlaneX * (xMax - xMin) * 0.5;

        vtkSmartPointer<vtkPlane> plane = vtkSmartPointer<vtkPlane>::New();
        plane->SetOrigin(planeX, 0.0, 0.0);
        plane->SetNormal(-1.0, 0.0, 0.0);

        clipFilter = vtkSmartPointer<vtkClipDataSet>::New();
        clipFilter->SetInputConnection(source);
        clipFilter->SetClipFunction(plane.Get());
        clipFilter->Update();
        source = clipFilter->GetOutputPort();
    }

    // Shrink filter operates on whatever's upstream.
    if (m_shrinkEnabled) {
        shrinkFilter = vtkSmartPointer<vtkShrinkFilter>::New();
        shrinkFilter->SetInputConnection(source);
        shrinkFilter->SetShrinkFactor(m_shrinkFactor);
        shrinkFilter->Update();
        source = shrinkFilter->GetOutputPort();
    }

    mapper->SetInputConnection(source);

    if (!actor)
        actor = vtkSmartPointer<vtkActor>::New();

    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(
        m_colour.redF(), m_colour.greenF(), m_colour.blueF());
    actor->SetVisibility(m_isVisible ? 1 : 0);
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

void ModelPart::computeExplodeDirection(double cx, double cy, double cz) {
    QVector3D scene(static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz));
    m_explodeDir = m_originalCentre - scene;
    if (m_explodeDir.length() > 0.0001f)
        m_explodeDir.normalize();
}

void ModelPart::applyExplode(double amount) {
    if (!actor)
        return;

    double bounds[6];
    actor->GetBounds(bounds);
    double dx = bounds[1] - bounds[0];
    double dy = bounds[3] - bounds[2];
    double dz = bounds[5] - bounds[4];
    double diag = std::sqrt(dx * dx + dy * dy + dz * dz);

    double mag = amount * diag * 1.5;
    actor->SetPosition(
        m_explodeDir.x() * mag,
        m_explodeDir.y() * mag,
        m_explodeDir.z() * mag);
}