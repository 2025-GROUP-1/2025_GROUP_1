/**
 * @file ModelPart.h
 * @brief Declares ModelPart, the data class for a single part in the tree.
 *
 * Each ModelPart owns its own VTK pipeline (STL reader -> optional filters
 * -> mapper -> actor) plus the data shown in the tree view (name, visibility).
 */

#ifndef MODELPART_H
#define MODELPART_H

#include <QList>
#include <QVariant>
#include <QString>
#include <QColor>

#include <vtkSmartPointer.h>
#include <vtkSTLReader.h>
#include <vtkDataSetMapper.h>
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
    /**
     * @brief Construct a new ModelPart.
     * @param data    Initial column data (typically [name, visibleString]).
     * @param parent  Parent node, or nullptr for the root.
     */
    ModelPart(const QList<QVariant>& data, ModelPart* parent = nullptr);

    /** @brief Destroys this part and recursively deletes its children. */
    ~ModelPart() noexcept;

    /**
     * @brief Add a child part.
     * @param item Child pointer; ownership is transferred.
     */
    void appendChild(ModelPart* item);

    /**
     * @brief Remove and delete the child at the given row.
     * @param row Row index.
     */
    void removeChild(int row);

    /** @return Child at the given row, or nullptr if out of range. */
    ModelPart* child(int row);

    /** @return Number of children. */
    int childCount() const;

    /** @return Number of data columns. */
    int columnCount() const;

    /**
     * @brief Get the data for a given column.
     * @param column 0 = name, 1 = visibility flag as string.
     */
    QVariant data(int column) const;

    /**
     * @brief Set the data for a given column.
     * @param column Column index.
     * @param value  New value.
     */
    void setData(int column, QVariant value);

    /** @return This node's row within its parent. */
    int row() const;

    /** @return Pointer to the parent node, or nullptr if root. */
    ModelPart* parentItem();

    /**
     * @brief Load an STL file and build the VTK pipeline.
     * @param fileName Absolute path to the STL file.
     * @return true on success, false otherwise.
     */
    bool loadSTL(QString fileName);

    /** @brief Rebuild the mapper/actor with the current filter settings. */
    void rebuildPipeline();

    /** @return The VTK actor (may be null if STL not yet loaded). */
    vtkSmartPointer<vtkActor> getActor();

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

    /**
     * @brief Set the shrink factor (0.1 strong shrink ... 1.0 no shrink).
     * @param factor Shrink factor in the range [0.1, 1.0].
     */
    void setShrinkFactor(double factor);

    /** @return The current shrink factor. */
    double getShrinkFactor() const;

    /** @brief Enable or disable the clip filter. */
    void setClipFilter(bool enabled);

    /** @return true if the clip filter is currently applied. */
    bool getClipEnabled() const;

    /**
     * @brief Set the X position of the clip plane.
     * @param x Plane offset along the X axis.
     */
    void setClipPlaneX(double x);

    /** @return The clip plane X position. */
    double getClipPlaneX() const;

    /** @brief Get STL bounds as [xmin, xmax, ymin, ymax, zmin, zmax]. */
    bool getBounds(double bounds[6]) const;

    /** @brief Apply temporary exploded-view translation to this actor. */
    void setExplodeOffset(double x, double y, double z);

    /** @brief Reset this actor to its assembled position. */
    void clearExplodeOffset();

private:
    QList<ModelPart*> m_childItems;   ///< Children in the tree.
    QList<QVariant>   m_itemData;     ///< Column data shown in the tree view.
    ModelPart* m_parentItem;   ///< Parent node, or nullptr if root.

    vtkSmartPointer<vtkSTLReader>     file;          ///< STL reader, source of the pipeline.
    vtkSmartPointer<vtkDataSetMapper> mapper;        ///< Mapper between filtered data and the actor.
    vtkSmartPointer<vtkActor>         actor;         ///< Actor placed in the renderer.
    vtkSmartPointer<vtkShrinkFilter>  shrinkFilter;  ///< Shrink filter (only built when enabled).
    vtkSmartPointer<vtkClipDataSet>   clipFilter;    ///< Clip filter (only built when enabled).

    QColor m_colour;          ///< RGB colour applied to the actor.
    bool   m_isVisible;       ///< Whether the part is shown in the renderer.
    bool   m_shrinkEnabled;   ///< Shrink filter on/off.
    double m_shrinkFactor;    ///< Shrink factor (0.1 strong - 1.0 none).
    bool   m_clipEnabled;     ///< Clip filter on/off.
    double m_clipPlaneX;      ///< Clip plane X offset.
    double m_explodeOffset[3]; ///< Current exploded-view translation.
};

#endif