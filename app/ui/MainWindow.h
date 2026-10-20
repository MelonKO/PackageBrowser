#pragma once

#include "app/ui/EnvironmentTreeModel.h"
#include "app/ui/InstallationTableModel.h"
#include "app/ui/PackageFilterProxy.h"
#include "app/ui/PluginListModel.h"

#include <QHash>
#include <QMainWindow>

class CatalogService;
class QCheckBox;
class QLineEdit;
class QTabWidget;
class QTreeView;
class QTableView;
class QListView;
class QPoint;
class QWidget;
class SettingsTab;

// Main application window: two tabs - "Packages" (environment tree + package
// table + filter) and "Plugins" (list of loaded providers with enable toggles).
// See docs/UI на Qt.md.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(CatalogService* catalog, QWidget* parent = nullptr);

private slots:
    void onEnvironmentSelected(const QModelIndex& index);
    void onFilterChanged(const QString& text);
    void onScanFinished();
    void onShowAllToggled(bool checked);
    void onReloadPlugins();
    void onTableContextMenu(const QPoint& pos);
    void showWhereInstalled(const QModelIndex& sourceIndex);

private:
    // envId -> label (root path) for the "where installed" dialog.
    QHash<QString, QString> environmentLabels() const;

    CatalogService* m_catalog;
    EnvironmentTreeModel m_treeModel;
    InstallationTableModel m_tableModel;
    PackageFilterProxy m_filterProxy;
    PluginListModel m_pluginModel;
    QTabWidget* m_tabs = nullptr;
    SettingsTab* m_settingsTab = nullptr;
    QTreeView* m_tree = nullptr;
    QTableView* m_table = nullptr;
    QListView* m_pluginList = nullptr;
    QLineEdit* m_filter = nullptr;
    QCheckBox* m_allToggle = nullptr;
};