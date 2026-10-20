#include "app/ui/InstallationTableModel.h"

InstallationTableModel::InstallationTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void InstallationTableModel::setInstallations(const QVector<Installation>& packages)
{
    beginResetModel();
    m_packages = packages;
    endResetModel();
}

void InstallationTableModel::setDependencies(const QVector<DependencyEdge>& dependencies)
{
    beginResetModel();
    m_dependencies.clear();
    for (const DependencyEdge& e : dependencies)
        m_dependencies[e.fromName()].append(e.toName());
    endResetModel();
}

const Installation* InstallationTableModel::packageAt(int row) const
{
    if (row < 0 || row >= m_packages.size())
        return nullptr;
    return &m_packages.at(row);
}

int InstallationTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_packages.size();
}

int InstallationTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return 5; // name, version, location, direct, depends-on
}

QVariant InstallationTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_packages.size())
        return QVariant();
    const Installation& p = m_packages[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return p.nameDisplay();
        case 1: return p.version();
        case 2: return p.location();
        case 3:
            if (p.isDirect().has_value())
                return p.isDirect().value() ? QStringLiteral("yes") : QStringLiteral("no");
            return QStringLiteral("-");
        case 4: {
            const QStringList deps = m_dependencies.value(p.nameNorm());
            return deps.isEmpty() ? QStringLiteral("-") : deps.join(QStringLiteral(", "));
        }
        }
    }
    return QVariant();
}

QVariant InstallationTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    switch (section) {
    case 0: return QStringLiteral("Name");
    case 1: return QStringLiteral("Version");
    case 2: return QStringLiteral("Location");
    case 3: return QStringLiteral("Direct");
    case 4: return QStringLiteral("Depends on");
    }
    return QVariant();
}