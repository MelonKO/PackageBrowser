#include "app/ui/MainWindow.h"

#include "catalog/CatalogService.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListView>
#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>
#include <QTableView>
#include <QTreeView>

MainWindow::MainWindow(CatalogService* catalog, QWidget* parent)
    : QMainWindow(parent)
    , m_catalog(catalog)
    , m_pluginModel(catalog->pluginHost(), &catalog->settings(), this)
{
    setWindowTitle(QStringLiteral("Package Browser"));

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    auto* filterRow = new QHBoxLayout;
    m_filter = new QLineEdit(central);
    m_filter->setPlaceholderText(QStringLiteral("Filter packages..."));
    m_allToggle = new QCheckBox(QStringLiteral("All packages"), central);
    filterRow->addWidget(m_filter, 1);
    filterRow->addWidget(m_allToggle);
    layout->addLayout(filterRow);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
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

    // Plugins screen: a list of loaded providers with an enabled checkbox.
    auto* pluginLabel = new QLabel(QStringLiteral("Plugins"), central);
    layout->addWidget(pluginLabel);
    m_pluginList = new QListView(central);
    m_pluginList->setModel(&m_pluginModel);
    m_pluginList->setUniformItemSizes(true);
    layout->addWidget(m_pluginList, 1);

    setCentralWidget(central);
    resize(1000, 720);

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