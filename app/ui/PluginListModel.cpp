#include "app/ui/PluginListModel.h"

#include "settings/AppSettings.h"

#include <QStringList>

PluginListModel::PluginListModel(AppSettings* settings, QObject* parent)
    : QAbstractListModel(parent)
    , m_settings(settings)
{
}

void PluginListModel::setPlugins(const QVector<PluginInfo>& plugins)
{
    beginResetModel();
    m_plugins = plugins;
    endResetModel();
}

int PluginListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_plugins.size();
}

QVariant PluginListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_plugins.size())
        return QVariant();

    const PluginInfo& info = m_plugins.at(index.row());
    switch (static_cast<Role>(role)) {
    case Role::PluginId:
        return info.id;
    case Role::Enabled:
        return isEnabledFromSettings(info.id);
    case Role::IsProcess:
        return info.isProcess;
    case Role::State:
        return info.state;
    case Role::Display:
        return info.id;
    default:
        return QVariant();
    }
}

bool PluginListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || role != Qt::EditRole || !m_settings
        || index.row() < 0 || index.row() >= m_plugins.size())
        return false;

    const QString pluginId = m_plugins.at(index.row()).id;
    QStringList disabled = m_settings->disabledPluginIds();
    const bool enable = value.toBool();
    const bool present = disabled.contains(pluginId);
    if (enable && present)
        disabled.removeAll(pluginId);
    else if (!enable && !present)
        disabled.append(pluginId);
    m_settings->setDisabledPluginIds(disabled);

    emit dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole, int(Role::Enabled) });
    return true;
}

Qt::ItemFlags PluginListModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = QAbstractListModel::flags(index);
    if (index.isValid())
        f |= Qt::ItemIsUserCheckable;
    return f;
}

bool PluginListModel::isEnabledFromSettings(const QString& pluginId) const
{
    return !m_settings->disabledPluginIds().contains(pluginId);
}