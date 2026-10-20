#include "app/ui/SettingsTab.h"

#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <functional>

namespace {

// Case-insensitive folder lookup so the same folder cannot be added twice with
// different casing (the scanner and TrustPolicy both treat paths as
// case-insensitive on Windows).
bool listContainsFolder(const QListWidget* list, const QString& dir)
{
    const QString normalized = QDir::cleanPath(dir);
    for (int i = 0; i < list->count(); ++i) {
        if (QDir::cleanPath(list->item(i)->text()).compare(
                normalized, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

// Write the current widget list back to AppSettings.
void persistList(AppSettings& settings,
                 void (AppSettings::*setter)(const QStringList&),
                 QListWidget* list)
{
    QStringList values;
    for (int i = 0; i < list->count(); ++i)
        values.append(list->item(i)->text());
    (settings.*setter)(values);
}

// Build a folder list with Add/Remove buttons, bound to one AppSettings
// QStringList (scan roots or trusted roots). Populated once at construction.
// onChanged fires after a successful add/remove so the owner can rescan.
QWidget* makeFolderGroup(AppSettings& settings,
                         QStringList (AppSettings::*getter)() const,
                         void (AppSettings::*setter)(const QStringList&),
                         QListWidget** outList,
                         std::function<void()> onChanged)
{
    auto* box = new QGroupBox;
    auto* boxLayout = new QVBoxLayout(box);

    auto* list = new QListWidget(box);
    *outList = list;
    const QStringList values = (settings.*getter)();
    for (const QString& v : values)
        list->addItem(v);
    boxLayout->addWidget(list);

    auto* row = new QHBoxLayout;
    auto* addBtn = new QPushButton(QStringLiteral("Add folder…"), box);
    auto* removeBtn = new QPushButton(QStringLiteral("Remove"), box);
    row->addWidget(addBtn);
    row->addWidget(removeBtn);
    row->addStretch(1);
    boxLayout->addLayout(row);
    removeBtn->setEnabled(false);

    QObject::connect(list, &QListWidget::itemSelectionChanged, box,
                     [removeBtn]() { removeBtn->setEnabled(true); });

    QObject::connect(addBtn, &QPushButton::clicked, box, [box, &settings, setter, list, onChanged]() {
        const QString dir = QFileDialog::getExistingDirectory(
            box, QStringLiteral("Select folder"));
        if (dir.isEmpty())
            return;
        if (!listContainsFolder(list, dir))
            list->addItem(dir);
        persistList(settings, setter, list);
        onChanged();
    });
    QObject::connect(removeBtn, &QPushButton::clicked, box, [removeBtn, &settings, setter, list, onChanged]() {
        const int row = list->currentRow();
        if (row < 0)
            return;
        delete list->takeItem(row);
        removeBtn->setEnabled(false);
        persistList(settings, setter, list);
        onChanged();
    });
    return box;
}

} // namespace

SettingsTab::SettingsTab(AppSettings& settings, QWidget* parent)
    : QWidget(parent)
    , m_settings(settings)
{
    auto* layout = new QVBoxLayout(this);

    auto* hint = new QLabel(
        QStringLiteral("Choose the project folders to scan for environments. "
                       "Trusted folders allow the manager tool to run (pip, npm); "
                       "others are read from files only, without executing them."),
        this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* group = new QGroupBox(QStringLiteral("Scan roots"), this);
    auto* groupLayout = new QVBoxLayout(group);
    auto* scanRootGroup = makeFolderGroup(
        m_settings, &AppSettings::scanRoots, &AppSettings::setScanRoots,
        &m_scanRootsList, [this]() { emit scanRequested(); });
    groupLayout->addWidget(scanRootGroup);
    layout->addWidget(group);

    auto* trustedGroup = new QGroupBox(QStringLiteral("Trusted roots"), this);
    auto* trustedLayout = new QVBoxLayout(trustedGroup);
    auto* trustedRootWidget = makeFolderGroup(
        m_settings, &AppSettings::trustedRoots, &AppSettings::setTrustedRoots,
        &m_trustedRootsList, [this]() { emit scanRequested(); });
    trustedLayout->addWidget(trustedRootWidget);
    layout->addWidget(trustedGroup);

    auto* depthRow = new QHBoxLayout;
    auto* depthLabel = new QLabel(QStringLiteral("Max scan depth:"), this);
    m_depthSpin = new QSpinBox(this);
    m_depthSpin->setRange(1, 10);
    {
        // Load the persisted depth without firing valueChanged (which would
        // trigger a spurious rescan before the tab is wired up).
        const QSignalBlocker blocker(m_depthSpin);
        m_depthSpin->setValue(m_settings.scanMaxDepth());
    }
    depthRow->addWidget(depthLabel);
    depthRow->addWidget(m_depthSpin);
    depthRow->addStretch(1);
    layout->addLayout(depthRow);

    auto* scanBtn = new QPushButton(QStringLiteral("Scan"), this);
    scanBtn->setDefault(true);
    layout->addWidget(scanBtn, 0, Qt::AlignRight);
    layout->addStretch(1);

    // Every change (roots, trusted roots, depth) and the explicit Scan button
    // trigger a rescan so the view reflects the new folders.
    connect(m_depthSpin, &QSpinBox::valueChanged, this, [this](int v) {
        m_settings.setScanMaxDepth(v);
        emit scanRequested();
    });
    connect(scanBtn, &QPushButton::clicked, this, &SettingsTab::scanRequested);
}