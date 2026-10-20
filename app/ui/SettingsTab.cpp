#include "app/ui/SettingsTab.h"

#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

// Build a folder list with Add/Remove buttons, bound to one AppSettings
// QStringList (scan roots or trusted roots). Populated once at construction.
QWidget* makeFolderGroup(AppSettings& settings,
                         QStringList (AppSettings::*getter)() const,
                         void (AppSettings::*setter)(const QStringList&),
                         QListWidget** outList)
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

    QObject::connect(addBtn, &QPushButton::clicked, box, [box, &settings, setter, list]() {
        const QString dir = QFileDialog::getExistingDirectory(
            box, QStringLiteral("Select folder"));
        if (dir.isEmpty())
            return;
        if (list->findItems(dir, Qt::MatchExactly).isEmpty())
            list->addItem(dir);
        QStringList values;
        for (int i = 0; i < list->count(); ++i)
            values.append(list->item(i)->text());
        (settings.*setter)(values);
    });
    QObject::connect(removeBtn, &QPushButton::clicked, box, [&settings, setter, list]() {
        const int row = list->currentRow();
        if (row < 0)
            return;
        delete list->takeItem(row);
        QStringList values;
        for (int i = 0; i < list->count(); ++i)
            values.append(list->item(i)->text());
        (settings.*setter)(values);
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
        &m_scanRootsList);
    groupLayout->addWidget(scanRootGroup);
    layout->addWidget(group);

    auto* trustedGroup = new QGroupBox(QStringLiteral("Trusted roots"), this);
    auto* trustedLayout = new QVBoxLayout(trustedGroup);
    auto* trustedRootWidget = makeFolderGroup(
        m_settings, &AppSettings::trustedRoots, &AppSettings::setTrustedRoots,
        &m_trustedRootsList);
    trustedLayout->addWidget(trustedRootWidget);
    layout->addWidget(trustedGroup);

    auto* depthRow = new QHBoxLayout;
    auto* depthLabel = new QLabel(QStringLiteral("Max scan depth:"), this);
    m_depthSpin = new QSpinBox(this);
    m_depthSpin->setRange(1, 10);
    m_depthSpin->setValue(m_settings.scanMaxDepth());
    depthRow->addWidget(depthLabel);
    depthRow->addWidget(m_depthSpin);
    depthRow->addStretch(1);
    layout->addLayout(depthRow);

    auto* scanBtn = new QPushButton(QStringLiteral("Scan"), this);
    scanBtn->setDefault(true);
    layout->addWidget(scanBtn, 0, Qt::AlignRight);
    layout->addStretch(1);

    // Persist the scan depth immediately; both the button and any change
    // trigger a rescan so the view reflects the new folders.
    connect(m_depthSpin, &QSpinBox::valueChanged, this, [this](int v) {
        m_settings.setScanMaxDepth(v);
        emit scanRequested();
    });
    connect(scanBtn, &QPushButton::clicked, this, &SettingsTab::scanRequested);
}