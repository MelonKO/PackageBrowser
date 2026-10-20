#include "app/ui/MainWindow.h"

#include "app/ui/SettingsTab.h"
#include "catalog/CatalogService.h"
#include "domain/Environment.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
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

    // Scan control + progress live where the results are shown.
    auto* scanRow = new QHBoxLayout;
    m_scanBtn = new QPushButton(QStringLiteral("Scan"), packagesPage);
    m_scanProgress = new QProgressBar(packagesPage);
    m_scanProgress->setRange(0, 100);
    m_scanProgress->setValue(0);
    m_scanProgress->setTextVisible(false);
    m_scanProgress->setVisible(false);
    scanRow->addWidget(m_scanBtn);
    scanRow->addWidget(m_scanProgress, 1);
    layout->addLayout(scanRow);

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

    // --- Settings tab ---
    m_settingsTab = new SettingsTab(catalog->settings(), m_tabs);
    m_tabs->addTab(m_settingsTab, QStringLiteral("Settings"));

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
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableView::customContextMenuRequested,
            this, &MainWindow::onTableContextMenu);

    // The Settings tab only persists and scans on explicit "Save Settings", so
    // edits while scanning are not lost to parallel rescans. Both paths (Save
    // on Settings and Scan on Packages) go through the same startScan() so the
    // button is disabled and no second scan can be queued while one runs.
    connect(m_settingsTab, &SettingsTab::saveAndScan, this, [this]() {
        onScanStarted();
        m_catalog->rescan();
    });

    // Scan control on the Packages tab, with progress feedback.
    connect(m_scanBtn, &QPushButton::clicked, this, [this]() {
        onScanStarted();
        m_catalog->rescan();
    });
    connect(m_catalog, &CatalogService::scanProgress,
            this, &MainWindow::onScanProgress);
    connect(m_catalog, &CatalogService::scanFinished, this, [this]() {
        m_scanProgress->setVisible(false);
        m_scanBtn->setEnabled(true);
    });
}

void MainWindow::onEnvironmentSelected(const QModelIndex& index)
{
    if (m_allToggle->isChecked())
        return;
    const Environment* env = m_treeModel.environmentAt(index);
    if (!env) {
        m_tableModel.setContent({}, {});
        return;
    }
    m_tableModel.setContent(m_catalog->listInstallations(env->id()),
                            m_catalog->dependencies(env->id()));
}

void MainWindow::onShowAllToggled(bool checked)
{
    if (checked) {
        // Flat list across all environments (edges are per-environment, so the
        // flat view shows none).
        m_tableModel.setContent(m_catalog->allPackages(), {});
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

void MainWindow::onTableContextMenu(const QPoint& pos)
{
    const QModelIndex index = m_filterProxy.mapToSource(m_table->indexAt(pos));
    if (!index.isValid())
        return;

    // Bring the clicked row under the mouse into focus so the follow-up action
    // targets it; the context menu must act on the row the user right-clicked,
    // not on whatever row happens to be the currently selected one.
    m_table->setCurrentIndex(index);

    QMenu menu(this);
    QAction* whereAction = menu.addAction(QStringLiteral("Where is this package installed?"));
    connect(whereAction, &QAction::triggered, this,
            [this, index]() { showWhereInstalled(index); });
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void MainWindow::showWhereInstalled(const QModelIndex& sourceIndex)
{
    const Installation* pkg = m_tableModel.packageAt(sourceIndex.row());
    if (!pkg)
        return;
    // "All packages" mode shows a flat list; elsewhere the current environment
    // is selected. findPackage answers "everywhere this package exists".
    const QVector<Installation> hits = m_catalog->findPackage(pkg->nameDisplay());
    if (hits.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Where installed"),
                                 QStringLiteral("Not found in any environment."));
        return;
    }

    QStringList lines;
    const QHash<QString, QString> envLabel = environmentLabels();
    for (const Installation& hit : hits) {
        const QString label = envLabel.value(hit.envId().value());
        lines.append(QStringLiteral("%1 (in %2)").arg(hit.nameDisplay(), label));
    }
    QMessageBox::information(this, QStringLiteral("Where installed"),
                             lines.join(QLatin1Char('\n')));
}

QHash<QString, QString> MainWindow::environmentLabels() const
{
    QHash<QString, QString> labels;
    for (const Environment& env : m_catalog->listEnvironments())
        labels.insert(env.id().value(), env.root());
    return labels;
}

void MainWindow::onScanStarted()
{
    m_scanProgress->setVisible(true);
    m_scanProgress->setValue(0);
    m_scanBtn->setEnabled(false);
}

void MainWindow::onScanProgress(int done, int total)
{
    if (!m_scanProgress->isVisible())
        m_scanProgress->setVisible(true);
    if (total > 0)
        m_scanProgress->setValue(static_cast<int>(100.0 * done / total));
}