/**
 * @file ModelPartList.h
 * @brief Tree model wrapping a hierarchy of ModelPart nodes for the QTreeView.
 */

#ifndef MODELPARTLIST_H
#define MODELPARTLIST_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QString>
#include <QList>

#include "ModelPart.h"

class ModelPart;

/**
 * @class ModelPartList
 * @brief QAbstractItemModel implementation backed by a tree of ModelPart objects.
 *
 * Provides the tree structure displayed in the GUI's QTreeView. Items can be
 * appended under any parent and removed individually.
 */
class ModelPartList : public QAbstractItemModel {
    Q_OBJECT

public:
    /**
     * @brief Construct the model with a single (invisible) root item.
     * @param data    Unused, kept for API symmetry.
     * @param parent  Optional QObject parent.
     */
    ModelPartList(const QString& data, QObject* parent = nullptr);

    /** @brief Destroys the model and the entire tree. */
    ~ModelPartList();

    /**
     * @brief Number of columns the tree displays.
     * @param parent Unused.
     */
    int columnCount(const QModelIndex& parent) const override;

    /**
     * @brief Get data for a cell in the tree.
     * @param index Row + column to fetch.
     * @param role  Qt role; only Qt::DisplayRole is handled.
     * @return The cell's value or an empty QVariant.
     */
    QVariant data(const QModelIndex& index, int role) const override;

    /**
     * @brief Item flags for a cell (interaction permissions).
     * @param index Cell to query.
     */
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    /**
     * @brief Header text for each column.
     * @param section     Column index.
     * @param orientation Horizontal / vertical.
     * @param role        Only Qt::DisplayRole is handled.
     */
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    /**
     * @brief Construct an index for a tree position.
     * @param row    Row under the parent.
     * @param column Column.
     * @param parent Parent index (invalid for root).
     */
    QModelIndex index(int row, int column, const QModelIndex& parent) const override;

    /**
     * @brief Find the parent index of a given index.
     * @param index Child index.
     */
    QModelIndex parent(const QModelIndex& index) const override;

    /**
     * @brief Number of children directly under `parent`.
     * @param parent Parent index.
     */
    int rowCount(const QModelIndex& parent) const override;

    /** @return Pointer to the (hidden) root item. */
    ModelPart* getRootItem();

    /**
     * @brief Append a new ModelPart under the given parent.
     * @param parent Parent index. If invalid, the new item goes under the root.
     * @param data   Initial column data for the new item.
     * @return Index of the newly created item.
     */
    QModelIndex appendChild(QModelIndex& parent, const QList<QVariant>& data);

    /**
     * @brief Remove and delete an item from the tree.
     * @param index Index of the item to remove.
     */
    void removeItem(const QModelIndex& index);

private:
    ModelPart* rootItem;   ///< Hidden root item; provides the column headers.
};

#endif