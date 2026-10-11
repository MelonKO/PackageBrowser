#pragma once

#include <QSortFilterProxyModel>

// Filter proxy over a package table: filters by free-text substring across
// all columns (name, version, location). See docs/UI на Qt.md.
class PackageFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit PackageFilterProxy(QObject* parent = nullptr);

    void setFilterText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_text;
};