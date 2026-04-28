#ifndef MODELPART_H
#define MODELPART_H

#include <QList>
#include <QVariant>
#include <QString>
#include <vtkSmartPointer.h>
#include <vtkSTLReader.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>

class ModelPart {
public:
    ModelPart(const QList<QVariant>& data, ModelPart* parent = nullptr);
    ~ModelPart();

    void appendChild(ModelPart* item);
    ModelPart* child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    int row() const;
    ModelPart* parentItem();
    void setData(int column, QVariant value);
    void loadSTL(QString fileName);
    vtkSmartPointer<vtkActor> getActor();
    void setColour(int r, int g, int b);
    int getColourR();
    int getColourG();
    int getColourB();
    void setVisible(bool visible);
    bool getVisible();

private:
    QList<ModelPart*>  m_childItems;
    QList<QVariant>    m_itemData;
    ModelPart*         m_parentItem;
    vtkSmartPointer<vtkSTLReader>      file;
    vtkSmartPointer<vtkPolyDataMapper> mapper;
    vtkSmartPointer<vtkActor>          actor;
    int  red;
    int  green;
    int  blue;
    bool isVisible;
};

#endif
