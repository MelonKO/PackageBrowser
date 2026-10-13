#pragma once

#include <QAbstractListModel>

class PluginHost;
class AppSettings;

// Rows for the plugins screen. Each row maps to a provider the host loaded
// (built-in pip, declarative manifests, process providers). The "enabled"
// checkbox is driven by AppSettings.disabledPluginIds() and persists there,
// so a toggled plugin is skipped on the next load.
// See docs/UI на Qt.md ("PluginListModel").
class PluginListModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit PluginListModel(PluginHost* host, AppSettings* settings, QObject* parent = nullptr);

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

    // Reverse-lookup for a provider: returns enabled state by plugin id.
    bool isEnabled(const QString& pluginId) const;

private:
    PluginHost* m_host;       // not owned
    AppSettings* m_settings;  // not owned
};