/** @file ModelPartList.h
 *  @brief Declares ModelPartList — the QAbstractItemModel that backs the QTreeView.
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

/** @brief Qt tree model backed by a hierarchy of ModelPart objects. */
class ModelPartList : public QAbstractItemModel {
    Q_OBJECT

public:
    ModelPartList(const QString& data, QObject* parent = nullptr);
    ~ModelPartList();

    // standard QAbstractItemModel overrides
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent) const override;

    // tree helpers
    ModelPart* getRootItem();
    ModelPart* findByID(int partID) const;
    QModelIndex appendChild(QModelIndex& parent, const QList<QVariant>& data);
    void removeItem(const QModelIndex& index);

private:
    ModelPart* rootItem;   // hidden root that holds the column headers
};

#endif
