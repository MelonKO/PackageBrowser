#pragma once

#include "domain/Installation.h"

#include <QAbstractTableModel>
#include <QVector>

// Table model of packages for a single environment (or all environments).
// See docs/UI на Qt.md.
class InstallationTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit InstallationTableModel(QObject* parent = nullptr);

    void setInstallations(const QVector<Installation>& packages);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    QVector<Installation> m_packages;
};