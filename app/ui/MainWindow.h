#pragma once

#include "app/ui/EnvironmentTreeModel.h"
#include "app/ui/InstallationTableModel.h"
#include "app/ui/PackageFilterProxy.h"
#include "app/ui/PluginListModel.h"

#include <QMainWindow>

class CatalogService;
class QCheckBox;
class QLineEdit;
class QTreeView;
class QTableView;
class QListView;

// Main application window: environment tree on the left, package table of the
// selected environment on the right, plus a filter box and a plugins screen.
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

private:
    CatalogService* m_catalog;
    EnvironmentTreeModel m_treeModel;
    InstallationTableModel m_tableModel;
    PackageFilterProxy m_filterProxy;
    PluginListModel m_pluginModel;
    QTreeView* m_tree = nullptr;
    QTableView* m_table = nullptr;
    QListView* m_pluginList = nullptr;
    QLineEdit* m_filter = nullptr;
    QCheckBox* m_allToggle = nullptr;
};