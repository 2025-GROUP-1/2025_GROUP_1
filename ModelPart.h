// represents a single STL part in the tree - holds the VTK pipeline, filters, and display state

#ifndef MODELPART_H
#define MODELPART_H

#include <QList>
#include <QVariant>
#include <QString>
#include <QColor>
#include <QVector3D>

#include <vtkSmartPointer.h>
#include <vtkSTLReader.h>
#include <vtkDataSetMapper.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkShrinkFilter.h>
#include <vtkClipDataSet.h>
#include <vtkPlane.h>
#include <vtkTransformFilter.h>
#include <vtkTransform.h>

class ModelPart {
public:
    ModelPart(const QList<QVariant>& data, ModelPart* parent = nullptr);
    ~ModelPart();

    // tree structure
    void appendChild(ModelPart* item);
    void removeChild(int row);
    ModelPart* child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    void setData(int column, QVariant value);
    int row() const;
    ModelPart* parentItem();

    // loading STL files
    bool loadSTL(QString fileName);
    bool attachReader(vtkSmartPointer<vtkSTLReader> preloaded);

    // rebuilds the VTK filter chain (clip -> shrink -> mapper -> actor)
    void rebuildPipeline();
    void rebuildVRPipeline();

    // actor access
    vtkSmartPointer<vtkActor> getActor();
    vtkActor* getVRActor() const;
    int getID() const;

    // display properties
    void setColour(const QColor& colour);
    QColor getColour() const;
    void setVisible(bool visible);
    bool getVisible() const;

    // shrink filter
    void setShrinkFilter(bool enabled);
    bool getShrinkEnabled() const;
    void setShrinkFactor(double factor);
    double getShrinkFactor() const;

    // clip filter
    void setClipFilter(bool enabled);
    bool getClipEnabled() const;
    void setClipPlaneX(double x);
    double getClipPlaneX() const;

    // explode view - each part moves radially outward from the scene centre
    void computeExplodeDirection(double cx, double cy, double cz);
    void applyExplode(double amount);

private:
    // tree
    QList<ModelPart*> m_childItems;
    QList<QVariant>   m_itemData;
    ModelPart* m_parentItem;

    // GUI VTK pipeline
    vtkSmartPointer<vtkSTLReader>     file;
    vtkSmartPointer<vtkDataSetMapper> mapper;
    vtkSmartPointer<vtkActor>         actor;
    vtkSmartPointer<vtkShrinkFilter>  shrinkFilter;
    vtkSmartPointer<vtkClipDataSet>   clipFilter;

    // VR pipeline (separate actor so VR and GUI don't fight)
    vtkSmartPointer<vtkDataSetMapper>   vrMapper;
    vtkSmartPointer<vtkActor>           vrActor;
    vtkSmartPointer<vtkShrinkFilter>    vrShrinkFilter;
    vtkSmartPointer<vtkClipDataSet>     vrClipFilter;
    vtkSmartPointer<vtkTransformFilter> vrScaleFilter;

    // state
    QColor m_colour;
    bool   m_isVisible;
    bool   m_shrinkEnabled;
    double m_shrinkFactor;
    bool   m_clipEnabled;
    double m_clipPlaneX;

    // explode view
    QVector3D m_originalCentre;
    QVector3D m_explodeDir;
};

#endif
