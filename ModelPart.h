/**
 * @file ModelPart.h
 * @brief Declares ModelPart, the data class for a single part in the tree.
 */

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

 /**
  * @class ModelPart
  * @brief Single node in the parts tree.
  */
class ModelPart {
public:
    /** @brief Construct a new ModelPart. */
    ModelPart(const QList<QVariant>& data, ModelPart* parent = nullptr);

    /** @brief Destroys this part and recursively deletes its children. */
    ~ModelPart();

    /** @brief Add a child part. Ownership is transferred. */
    void appendChild(ModelPart* item);

    /** @brief Remove and delete the child at the given row. */
    void removeChild(int row);

    /** @return Child at the given row, or nullptr if out of range. */
    ModelPart* child(int row);

    /** @return Number of children. */
    int childCount() const;

    /** @return Number of data columns. */
    int columnCount() const;

    /** @brief Get the data for a given column. */
    QVariant data(int column) const;

    /** @brief Set the data for a given column. */
    void setData(int column, QVariant value);

    /** @return This node's row within its parent. */
    int row() const;

    /** @return Pointer to the parent node, or nullptr if root. */
    ModelPart* parentItem();

    /** @brief Load an STL file and build the VTK pipeline. */
    bool loadSTL(QString fileName);

    /** @brief Rebuild the mapper/actor with the current filter settings. */
    void rebuildPipeline();

    /** @brief Rebuild the VR actor pipeline (called by VRRenderThread on property change). */
    void rebuildVRPipeline();

    /** @return The VTK actor for the GUI viewport (may be null if STL not yet loaded). */
    vtkSmartPointer<vtkActor> getActor();

    /** @return The VTK actor for the VR renderer (raw pointer, valid while part exists). */
    vtkActor* getVRActor() const;

    /** @return Unique integer ID for this part, used by VRRenderThread. */
    int getID() const;

    /** @brief Set the part's display colour. */
    void setColour(const QColor& colour);

    /** @return The current colour. */
    QColor getColour() const;

    /** @brief Set whether the part is rendered. */
    void setVisible(bool visible);

    /** @return true if currently visible. */
    bool getVisible() const;

    /** @brief Enable or disable the shrink filter. */
    void setShrinkFilter(bool enabled);

    /** @return true if the shrink filter is currently applied. */
    bool getShrinkEnabled() const;

    /** @brief Set the shrink factor (0.1 strong shrink ... 1.0 no shrink). */
    void setShrinkFactor(double factor);

    /** @return The current shrink factor. */
    double getShrinkFactor() const;

    /** @brief Enable or disable the clip filter. */
    void setClipFilter(bool enabled);

    /** @return true if the clip filter is currently applied. */
    bool getClipEnabled() const;

    /** @brief Set the X position of the clip plane. */
    void setClipPlaneX(double x);

    /** @return The clip plane X position. */
    double getClipPlaneX() const;

    /** @brief Compute the radial offset direction for explode view. */
    void computeExplodeDirection(double cx, double cy, double cz);

    /** @brief Apply the current explode amount (0.0 = original, 1.0 = full). */
    void applyExplode(double amount);

private:
    QList<ModelPart*> m_childItems;   ///< Children in the tree.
    QList<QVariant>   m_itemData;     ///< Column data shown in the tree view.
    ModelPart* m_parentItem;   ///< Parent node, or nullptr if root.

    vtkSmartPointer<vtkSTLReader>     file;          ///< STL reader, source of the pipeline.
    vtkSmartPointer<vtkDataSetMapper> mapper;        ///< Mapper between filtered data and the GUI actor.
    vtkSmartPointer<vtkActor>         actor;         ///< Actor placed in the GUI renderer.
    vtkSmartPointer<vtkShrinkFilter>  shrinkFilter;  ///< Shrink filter (only built when enabled).
    vtkSmartPointer<vtkClipDataSet>   clipFilter;    ///< Clip filter (only built when enabled).
    vtkSmartPointer<vtkPolyDataMapper> vrMapper;     ///< Direct mapper for VR (no filters).
    vtkSmartPointer<vtkActor>          vrActor;      ///< Actor for the VR renderer.

    QColor m_colour;            ///< RGB colour applied to the actor.
    bool   m_isVisible;         ///< Whether the part is shown.
    bool   m_shrinkEnabled;     ///< Shrink filter on/off.
    double m_shrinkFactor;      ///< Shrink factor (0.1 - 1.0).
    bool   m_clipEnabled;       ///< Clip filter on/off.
    double m_clipPlaneX;        ///< Clip plane X offset.

    QVector3D m_originalCentre; ///< Centre of mass at load time, used for explode.
    QVector3D m_explodeDir;     ///< Unit vector pointing away from scene centre.
};

#endif