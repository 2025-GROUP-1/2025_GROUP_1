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

/**
 * @brief Represents a single CAD part in the application's tree model.
 *
 * Each ModelPart is a node in the hierarchy displayed by the QTreeView. It stores
 * per-column display data (part name and visibility) and owns a VTK pipeline —
 * STL reader → poly-data mapper → actor — used to render the geometry in the 3D
 * viewport. Colour defaults to white (255, 255, 255) and visibility defaults to true.
 *
 * @see ModelPartList
 */
class ModelPart {
public:
    /**
     * @brief Constructs a ModelPart with the given column data and optional parent.
     *
     * Colour is initialised to white (255, 255, 255) and visibility to true.
     *
     * @param data  Column values for the tree view, typically
     *              \c {partName, "true"}.
     * @param parent Pointer to the parent ModelPart, or \c nullptr for a root node.
     */
    ModelPart(const QList<QVariant>& data, ModelPart* parent = nullptr);

    /**
     * @brief Destructor. Recursively deletes all child ModelPart items.
     */
    ~ModelPart();

    /**
     * @brief Appends a child item and sets its parent pointer to this item.
     * @param item Pointer to the child to append. Ownership is transferred.
     */
    void appendChild(ModelPart* item);

    /**
     * @brief Returns the child at the given row index.
     * @param row Zero-based index into the child list.
     * @return Pointer to the child, or \c nullptr if @p row is out of range.
     */
    ModelPart* child(int row);

    /**
     * @brief Returns the number of direct children of this item.
     * @return Child count (0 if this is a leaf node).
     */
    int childCount() const;

    /**
     * @brief Returns the number of data columns this item has.
     * @return Size of the internal data list (normally 2: name and visibility).
     */
    int columnCount() const;

    /**
     * @brief Returns the display data for the given column.
     *
     * Column 0 returns the part name. Column 1 returns the string \c "true" or
     * \c "false" reflecting the current visibility state (not the raw stored value).
     *
     * @param column 0 for name, 1 for visibility.
     * @return QVariant containing the value, or an invalid QVariant if @p column
     *         is out of range.
     */
    QVariant data(int column) const;

    /**
     * @brief Returns this item's row index within its parent's child list.
     * @return Zero-based row, or 0 if this item has no parent.
     */
    int row() const;

    /**
     * @brief Returns a pointer to the parent ModelPart.
     * @return Parent pointer, or \c nullptr if this is a root-level item.
     */
    ModelPart* parentItem();

    /**
     * @brief Replaces the stored value at the given column.
     *
     * Does nothing if @p column is out of range.
     *
     * @param column Column index to update.
     * @param value  New value to store.
     */
    void setData(int column, QVariant value);

    /**
     * @brief Loads an STL file and builds the VTK rendering pipeline.
     *
     * Creates a vtkSTLReader → vtkPolyDataMapper → vtkActor chain. The actor is
     * initialised with the current colour and visibility values.  If loadSTL()
     * is called again on the same part, the existing pipeline is replaced.
     *
     * @param fileName Absolute path to the STL file.
     * @see getActor()
     */
    void loadSTL(QString fileName);

    /**
     * @brief Returns the VTK actor for this part.
     *
     * The actor is \c null until loadSTL() has been called successfully.
     *
     * @return Smart pointer to the actor; may be a null smart pointer.
     * @see loadSTL()
     */
    vtkSmartPointer<vtkActor> getActor();

    /**
     * @brief Stores the RGB colour for this part.
     *
     * The stored values are applied to the VTK actor when loadSTL() is next
     * called.  To update a live actor's colour, call loadSTL() again after
     * setColour().
     *
     * @param r Red component (0–255).
     * @param g Green component (0–255).
     * @param b Blue component (0–255).
     * @see getColourR(), getColourG(), getColourB()
     */
    void setColour(int r, int g, int b);

    /**
     * @brief Returns the stored red colour component.
     * @return Red value in the range 0–255.
     * @see setColour()
     */
    int getColourR();

    /**
     * @brief Returns the stored green colour component.
     * @return Green value in the range 0–255.
     * @see setColour()
     */
    int getColourG();

    /**
     * @brief Returns the stored blue colour component.
     * @return Blue value in the range 0–255.
     * @see setColour()
     */
    int getColourB();

    /**
     * @brief Sets the visibility of this part.
     *
     * Updates the internal flag, synchronises column 1 of the tree data to
     * \c "true" or \c "false", and immediately updates the VTK actor visibility
     * if an actor exists.
     *
     * @param visible \c true to show the part, \c false to hide it.
     * @see getVisible()
     */
    void setVisible(bool visible);

    /**
     * @brief Returns the current visibility state.
     * @return \c true if the part is visible, \c false if hidden.
     * @see setVisible()
     */
    bool getVisible();

private:
    QList<ModelPart*>  m_childItems; ///< Ordered list of child nodes; owned by this item.
    QList<QVariant>    m_itemData;   ///< Per-column data: index 0 = name, index 1 = visibility string.
    ModelPart*         m_parentItem; ///< Non-owning pointer to the parent; nullptr for root nodes.

    vtkSmartPointer<vtkSTLReader>      file;   ///< VTK reader for the STL geometry file.
    vtkSmartPointer<vtkPolyDataMapper> mapper; ///< VTK mapper connecting the reader output to the actor.
    vtkSmartPointer<vtkActor>          actor;  ///< VTK actor added to the renderer; null until loadSTL() is called.

    int  red;       ///< Red colour component (0–255); defaults to 255.
    int  green;     ///< Green colour component (0–255); defaults to 255.
    int  blue;      ///< Blue colour component (0–255); defaults to 255.
    bool isVisible; ///< Visibility flag; true by default. Mirrored to the VTK actor on setVisible().
};

#endif
