/**
 * @file ModelPartList.h
 *
 * EEEE2076 - Software Engineering & VR Project
 *
 * Qt tree model that owns and manages the ModelPart hierarchy.
 *
 * P Evans 2022
 */

#ifndef VIEWER_MODELPARTLIST_H
#define VIEWER_MODELPARTLIST_H

#include "ModelPart.h"

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QString>
#include <QList>

class ModelPart;

/**
 * @brief Qt tree model that manages the hierarchy of ModelPart nodes shown in the tree view.
 *
 * Implements QAbstractItemModel so that a QTreeView can consume it directly.  The tree
 * always has exactly two columns: "Part" (column 0, the part name) and "Visible?"
 * (column 1, showing "true" or "false").  An invisible root ModelPart acts as the
 * column-header node; all user-visible items are its descendants.
 *
 * @see ModelPart, MainWindow
 */
class ModelPartList : public QAbstractItemModel {
    Q_OBJECT ///< Required Qt macro enabling signals, slots, and the meta-object system.

public:
    /**
     * @brief Constructs the model and creates the invisible root item.
     *
     * The @p data argument is accepted for API compatibility but is not used; the
     * root item is always initialised with columns "Part" and "Visible?".
     *
     * @param data   Unused; present for interface compatibility.
     * @param parent Optional parent QObject for memory management.
     */
    ModelPartList(const QString& data, QObject* parent = nullptr);

    /**
     * @brief Destructor. Deletes the root ModelPart and its entire subtree.
     */
    ~ModelPartList();

    /**
     * @brief Returns the number of columns in the tree view.
     *
     * Always returns 2 ("Part" and "Visible?"), regardless of @p parent.
     *
     * @param parent Unused; present to satisfy the QAbstractItemModel interface.
     * @return Number of columns (always 2).
     */
    int columnCount(const QModelIndex& parent) const;

    /**
     * @brief Returns the display data for the item at @p index.
     *
     * Delegates to ModelPart::data() for the requested column.  Returns an
     * empty QVariant for an invalid index or for any role other than
     * Qt::DisplayRole.
     *
     * @param index Model index specifying the row and column.
     * @param role  Qt data role; only Qt::DisplayRole is handled.
     * @return QVariant containing the string value, or an invalid QVariant.
     */
    QVariant data(const QModelIndex& index, int role) const;

    /**
     * @brief Returns the item flags for @p index.
     *
     * Returns Qt::NoItemFlags for an invalid index; otherwise delegates to
     * QAbstractItemModel::flags().
     *
     * @param index Model index to query.
     * @return Item flags for the specified index.
     */
    Qt::ItemFlags flags(const QModelIndex& index) const;

    /**
     * @brief Returns the header label for a given section and orientation.
     *
     * Returns the root item's column data for horizontal DisplayRole requests
     * ("Part" for section 0, "Visible?" for section 1).  Returns an empty
     * QVariant for all other orientations and roles.
     *
     * @param section     Column index (0 or 1).
     * @param orientation Qt::Horizontal or Qt::Vertical.
     * @param role        Qt data role; only Qt::DisplayRole is handled.
     * @return QVariant containing the header string, or an invalid QVariant.
     */
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    /**
     * @brief Creates a QModelIndex for the item at @p row / @p column under @p parent.
     *
     * If @p parent is invalid, the root item is used as the parent.  Returns an
     * invalid QModelIndex if the child does not exist.
     *
     * @param row    Zero-based row index under @p parent.
     * @param column 0 for "Part", 1 for "Visible?".
     * @param parent Index of the parent item; pass an invalid index for top-level items.
     * @return QModelIndex for the requested cell, or an invalid index if not found.
     */
    QModelIndex index(int row, int column, const QModelIndex& parent) const;

    /**
     * @brief Returns the parent index of the item at @p index.
     *
     * Returns an invalid QModelIndex if @p index is invalid or if the item's
     * parent is the invisible root (i.e. the item is a top-level node).
     *
     * @param index Index of the child item.
     * @return QModelIndex of the parent, or an invalid index for top-level items.
     */
    QModelIndex parent(const QModelIndex& index) const;

    /**
     * @brief Returns the number of child rows under @p parent.
     *
     * If @p parent is invalid, returns the number of top-level items.  Returns 0
     * when @p parent refers to a column other than 0 (Qt convention).
     *
     * @param parent Index of the parent item; pass an invalid index for the root level.
     * @return Number of direct children.
     */
    int rowCount(const QModelIndex& parent) const;

    /**
     * @brief Returns a pointer to the invisible root ModelPart.
     *
     * The root item is not displayed in the tree view; it serves as the parent of
     * all top-level parts.
     *
     * @return Non-null pointer to the root ModelPart.
     */
    ModelPart* getRootItem();

    /**
     * @brief Creates a new child ModelPart and appends it under @p parent.
     *
     * Inserts the new item using Qt's beginInsertRows/endInsertRows protocol so that
     * connected views update automatically.  Emits layoutChanged() after insertion.
     * If @p parent is invalid, the item is appended at the top level under the root.
     *
     * @param parent Model index of the intended parent; pass an invalid index for a top-level item.
     * @param data   Column values for the new ModelPart (typically name and visibility string).
     * @return QModelIndex of the newly created child.
     */
    QModelIndex appendChild(QModelIndex& parent, const QList<QVariant>& data);

private:
    ModelPart* rootItem; ///< Invisible root node; acts as the parent of all top-level parts.
};

#endif
