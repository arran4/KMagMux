#ifndef ITEMFILTERPROXYMODEL_H
#define ITEMFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QString>

class ItemFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ItemFilterProxyModel(QObject *parent = nullptr);

    void setFilterText(const QString &text);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    QString m_filterText;
};

#endif // ITEMFILTERPROXYMODEL_H
