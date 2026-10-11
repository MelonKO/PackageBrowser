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
    return 4; // name, version, location, direct
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
    }
    return QVariant();
}