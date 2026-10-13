#include "app/ui/MainWindow.h"

#include "catalog/CatalogService.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListView>
#include <QLabel>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QTableView>
#include <QTreeView>

MainWindow::MainWindow(CatalogService* catalog, QWidget* parent)
    : QMainWindow(parent)
    , m_catalog(catalog)
    , m_pluginModel(&catalog->settings(), this)
{
    setWindowTitle(QStringLiteral("Package Browser"));

    // Two tabs: Packages (browser) and Plugins (providers screen).
    m_tabs = new QTabWidget(this);

    // --- Packages tab ---
    auto* packagesPage = new QWidget(m_tabs);
    auto* layout = new QVBoxLayout(packagesPage);

    auto* filterRow = new QHBoxLayout;
    m_filter = new QLineEdit(packagesPage);
    m_filter->setPlaceholderText(QStringLiteral("Filter packages..."));
    m_allToggle = new QCheckBox(QStringLiteral("All packages"), packagesPage);
    filterRow->addWidget(m_filter, 1);
    filterRow->addWidget(m_allToggle);
    layout->addLayout(filterRow);

    auto* splitter = new QSplitter(Qt::Horizontal, packagesPage);
    m_tree = new QTreeView(splitter);
    m_tree->setModel(&m_treeModel);
    m_tree->setHeaderHidden(true);

    m_table = new QTableView(splitter);
    m_filterProxy.setSourceModel(&m_tableModel);
    m_table->setModel(&m_filterProxy);
    m_table->horizontalHeader()->setStretchLastSection(true);
    splitter->addWidget(m_tree);
    splitter->addWidget(m_table);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    layout->addWidget(splitter);

    m_tabs->addTab(packagesPage, QStringLiteral("Packages"));

    // --- Plugins tab ---
    auto* pluginsPage = new QWidget(m_tabs);
    auto* pluginsLayout = new QVBoxLayout(pluginsPage);
    auto* pluginsHint = new QLabel(
        QStringLiteral("Loaded package managers. Disabling a plugin skips it on the next scan."),
        pluginsPage);
    pluginsHint->setWordWrap(true);
    pluginsLayout->addWidget(pluginsHint);
    m_pluginList = new QListView(pluginsPage);
    m_pluginList->setModel(&m_pluginModel);
    m_pluginList->setUniformItemSizes(true);
    pluginsLayout->addWidget(m_pluginList);
    m_tabs->addTab(pluginsPage, QStringLiteral("Plugins"));

    setCentralWidget(m_tabs);
    resize(1000, 700);

    // Prime the plugins tab with whatever snapshot is already available, then
    // refresh after each scan completes.
    m_pluginModel.setPlugins(m_catalog->plugins());

    // Show current environments, then schedule a scan.
    m_treeModel.setEnvironments(m_catalog->listEnvironments());

    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex& current, const QModelIndex&) {
                onEnvironmentSelected(current);
            });
    connect(m_filter, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(m_allToggle, &QCheckBox::toggled, this, &MainWindow::onShowAllToggled);
    connect(m_catalog, &CatalogService::environmentsChanged, this, [this]() {
        m_treeModel.setEnvironments(m_catalog->listEnvironments());
    });
    connect(m_catalog, &CatalogService::pluginsChanged, this, &MainWindow::onReloadPlugins);
    connect(m_catalog, &CatalogService::scanFinished, this, &MainWindow::onScanFinished);
}

void MainWindow::onEnvironmentSelected(const QModelIndex& index)
{
    if (m_allToggle->isChecked())
        return;
    const Environment* env = m_treeModel.environmentAt(index);
    if (!env) {
        m_tableModel.setInstallations({});
        return;
    }
    m_tableModel.setInstallations(m_catalog->listInstallations(env->id()));
}

void MainWindow::onShowAllToggled(bool checked)
{
    if (checked) {
        // Flat list across all environments.
        m_tableModel.setInstallations(m_catalog->allPackages());
        m_allToggle->setText(QStringLiteral("All packages (*)"));
    } else {
        m_allToggle->setText(QStringLiteral("All packages"));
        const QModelIndex current = m_tree->currentIndex();
        if (current.isValid())
            onEnvironmentSelected(current);
    }
}

void MainWindow::onFilterChanged(const QString& text)
{
    // The proxy filters the current table with substring semantics in every
    // mode, so "All packages" and per-environment views behave consistently.
    m_filterProxy.setFilterText(text);
}

void MainWindow::onScanFinished()
{
    const QModelIndex current = m_tree->currentIndex();
    if (current.isValid() && !m_allToggle->isChecked())
        onEnvironmentSelected(current);
}

void MainWindow::onReloadPlugins()
{
    // Rebuild the plugins list from the latest worker snapshot. The enabled
    // checkbox still reads AppSettings (the source of truth), and toggling it
    // goes back through settings, so this resync only refreshes ids/states.
    m_pluginModel.setPlugins(m_catalog->plugins());
}