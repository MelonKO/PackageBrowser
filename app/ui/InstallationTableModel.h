#pragma once

#include "domain/DependencyEdge.h"
#include "domain/Installation.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QVector>

// Table model of packages for a single environment (or all environments).
// Dependency edges of the environment are shown as a "Depends on" column for
// packages that declare dependencies.
// See docs/UI на Qt.md.
class InstallationTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit InstallationTableModel(QObject* parent = nullptr);

    void setInstallations(const QVector<Installation>& packages);
    void setDependencies(const QVector<DependencyEdge>& dependencies);
    // Set packages and their dependency edges together (single model reset).
    void setContent(const QVector<Installation>& packages,
                    const QVector<DependencyEdge>& dependencies);

    // Package at the given display row; null when row is out of range.
    const Installation* packageAt(int row) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    QVector<Installation> m_packages;
    // nameNorm -> display list of direct dependencies (for the "Depends on" col).
    QHash<QString, QStringList> m_dependencies;
};