#include "app/ui/PluginListModel.h"

#include "plugin/PluginHost.h"
#include "provider/ProcessProvider.h"
#include "settings/AppSettings.h"

#include <QStringList>

namespace {
QString stateText(int s)
{
    switch (static_cast<ProcessProviderState>(s)) {
    case ProcessProviderState::Ready: return QStringLiteral("ready");
    case ProcessProviderState::Failed: return QStringLiteral("failed");
    case ProcessProviderState::Quarantined: return QStringLiteral("quarantined");
    }
    return QStringLiteral("ready");
}
} // namespace

PluginListModel::PluginListModel(PluginHost* host, AppSettings* settings, QObject* parent)
    : QAbstractListModel(parent)
    , m_host(host)
    , m_settings(settings)
{
}

int PluginListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !m_host)
        return 0;
    return m_host->providers().size();
}

QVariant PluginListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || !m_host)
        return QVariant();

    const IProvider* provider = m_host->providers().at(index.row());
    switch (static_cast<Role>(role)) {
    case Role::PluginId:
        return provider->pluginId().value();
    case Role::Enabled:
        return m_settings->disabledPluginIds().contains(provider->pluginId().value())
            ? false : true;
    case Role::IsProcess:
        return dynamic_cast<const ProcessProvider*>(provider) != nullptr;
    case Role::State:
        if (const auto* pp = dynamic_cast<const ProcessProvider*>(provider))
            return stateText(int(pp->state()));
        return QStringLiteral("ready");
    case Role::Display:
        return provider->kind().pluginId().value();
    default:
        return QVariant();
    }
}

bool PluginListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || role != Qt::EditRole || !m_host)
        return false;

    const QString pluginId = m_host->providers().at(index.row())->pluginId().value();
    QStringList disabled = m_settings->disabledPluginIds();
    const bool enable = value.toBool();
    const bool present = disabled.contains(pluginId);
    if (enable && present)
        disabled.removeAll(pluginId);
    else if (!enable && !present)
        disabled.append(pluginId);
    m_settings->setDisabledPluginIds(disabled);

    // Refresh the row so the checkbox reflects the new state.
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

bool PluginListModel::isEnabled(const QString& pluginId) const
{
    return !m_settings->disabledPluginIds().contains(pluginId);
}