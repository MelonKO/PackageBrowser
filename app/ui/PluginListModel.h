#pragma once

#include "catalog/PluginInfo.h"

#include <QAbstractListModel>
#include <QVector>

class AppSettings;

// Rows for the plugins screen. Each row maps to a loaded provider described by
// a PluginInfo snapshot that the worker delivers after a scan, so the UI never
// touches a live PluginHost across threads. The "enabled" checkbox is driven by
// AppSettings.disabledPluginIds() (persisted), and toggle calls back into the
// settings to add/remove the plugin id.
// See docs/UI на Qt.md ("PluginListModel").
class PluginListModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit PluginListModel(AppSettings* settings, QObject* parent = nullptr);

    // Replace the displayed providers from a fresh snapshot.
    void setPlugins(const QVector<PluginInfo>& plugins);

    enum class Role {
        Display = Qt::DisplayRole,
        PluginId = Qt::UserRole + 1,
        State,
        Enabled,
        IsProcess
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    // Enabled state as persisted in settings (the snapshot's `enabled` is
    // informational; the checkbox reads the source of truth).
    bool isEnabledFromSettings(const QString& pluginId) const;

    AppSettings* m_settings;  // not owned
    QVector<PluginInfo> m_plugins;
};