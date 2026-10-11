#include "app/ui/PackageFilterProxy.h"

PackageFilterProxy::PackageFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSortRole(Qt::DisplayRole);
    setDynamicSortFilter(true);
}

void PackageFilterProxy::setFilterText(const QString& text)
{
    m_text = text.trimmed();
    invalidateFilter();
}

bool PackageFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    Q_UNUSED(sourceParent);
    if (m_text.isEmpty())
        return true;
    for (int col = 0; col < sourceModel()->columnCount(); ++col) {
        const QModelIndex idx = sourceModel()->index(sourceRow, col);
        if (sourceModel()->data(idx).toString().contains(m_text, Qt::CaseInsensitive))
            return true;
    }
    return false;
}