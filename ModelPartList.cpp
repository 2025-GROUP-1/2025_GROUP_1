/**
 * @file ModelPartList.cpp
 * @brief Implements tree indexing, insertion, removal, and lookup for the CAD part model.
 */

#include "ModelPartList.h"

ModelPartList::ModelPartList(const QString& data, QObject* parent)
    : QAbstractItemModel(parent)
{
    Q_UNUSED(data);
    // root item is never shown, just provides column headers
    rootItem = new ModelPart({ tr("Part"), tr("Visible?") });
}

ModelPartList::~ModelPartList() {
    delete rootItem;
}

int ModelPartList::columnCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return rootItem->columnCount();
}

QVariant ModelPartList::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return QVariant();
    if (role != Qt::DisplayRole)
        return QVariant();

    ModelPart* item = static_cast<ModelPart*>(index.internalPointer());
    return item->data(index.column());
}

Qt::ItemFlags ModelPartList::flags(const QModelIndex& index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;
    return QAbstractItemModel::flags(index);
}

QVariant ModelPartList::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return rootItem->data(section);
    return QVariant();
}

// creates an index for a specific row/column under a parent
QModelIndex ModelPartList::index(int row, int column, const QModelIndex& parent) const {
    ModelPart* parentItem;

    if (!parent.isValid() || !hasIndex(row, column, parent))
        parentItem = rootItem;
    else
        parentItem = static_cast<ModelPart*>(parent.internalPointer());

    ModelPart* childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);

    return QModelIndex();
}

// walks up to find the parent index
QModelIndex ModelPartList::parent(const QModelIndex& index) const {
    if (!index.isValid())
        return QModelIndex();

    ModelPart* childItem = static_cast<ModelPart*>(index.internalPointer());
    ModelPart* parentItem = childItem->parentItem();

    if (parentItem == rootItem || parentItem == nullptr)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int ModelPartList::rowCount(const QModelIndex& parent) const {
    if (parent.column() > 0)
        return 0;

    ModelPart* parentItem = parent.isValid()
        ? static_cast<ModelPart*>(parent.internalPointer())
        : rootItem;

    return parentItem->childCount();
}

ModelPart* ModelPartList::getRootItem() {
    return rootItem;
}

// adds a new child under the given parent and notifies the view
QModelIndex ModelPartList::appendChild(QModelIndex& parent, const QList<QVariant>& data) {
    ModelPart* parentPart = parent.isValid()
        ? static_cast<ModelPart*>(parent.internalPointer())
        : rootItem;

    int newRow = parentPart->childCount();

    beginInsertRows(parent, newRow, newRow);
    ModelPart* childPart = new ModelPart(data, parentPart);
    parentPart->appendChild(childPart);
    endInsertRows();

    return createIndex(newRow, 0, childPart);
}

// BFS search for a part by its unique ID
ModelPart* ModelPartList::findByID(int partID) const {
    QList<ModelPart*> stack;
    stack.append(rootItem);
    while (!stack.isEmpty()) {
        ModelPart* item = stack.takeFirst();
        if (item != rootItem && item->getID() == partID)
            return item;
        for (int i = 0; i < item->childCount(); ++i)
            stack.append(item->child(i));
    }
    return nullptr;
}

// removes a part from the tree and deletes it
void ModelPartList::removeItem(const QModelIndex& index) {
    if (!index.isValid())
        return;

    ModelPart* item = static_cast<ModelPart*>(index.internalPointer());
    if (!item)
        return;

    ModelPart* parentItem = item->parentItem();
    if (!parentItem)
        parentItem = rootItem;

    int row = item->row();
    QModelIndex parentIndex = parent(index);

    beginRemoveRows(parentIndex, row, row);
    parentItem->removeChild(row);
    endRemoveRows();
}
