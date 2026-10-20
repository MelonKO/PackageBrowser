#include "app/ui/SettingsTab.h"

#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
enum RootRole {
    TrustedRole = Qt::UserRole  // QListWidgetItem flag: true = trusted
};
} // namespace

SettingsTab::SettingsTab(AppSettings& settings, QWidget* parent)
    : QWidget(parent)
    , m_settings(settings)
{
    auto* layout = new QVBoxLayout(this);

    auto* hint = new QLabel(
        QStringLiteral("Project folders to scan for environments. Mark a folder "
                       "as trusted to let its manager tool run (pip, npm); "
                       "untrusted folders are read from files only."),
        this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_rootsList = new QListWidget(this);
    m_rootsList->setAlternatingRowColors(true);
    layout->addWidget(m_rootsList);

    auto* row = new QHBoxLayout;
    auto* addBtn = new QPushButton(QStringLiteral("Add folder…"), this);
    auto* removeBtn = new QPushButton(QStringLiteral("Remove"), this);
    row->addWidget(addBtn);
    row->addWidget(removeBtn);
    row->addStretch(1);
    layout->addLayout(row);

    auto* depthRow = new QHBoxLayout;
    auto* depthLabel = new QLabel(QStringLiteral("Max scan depth:"), this);
    m_depthSpin = new QSpinBox(this);
    m_depthSpin->setRange(1, 10);
    depthRow->addWidget(depthLabel);
    depthRow->addWidget(m_depthSpin);
    depthRow->addStretch(1);
    layout->addLayout(depthRow);

    auto* saveBtn = new QPushButton(QStringLiteral("Save Settings"), this);
    saveBtn->setDefault(true);
    layout->addWidget(saveBtn, 0, Qt::AlignRight);
    layout->addStretch(1);

    connect(addBtn, &QPushButton::clicked, this, &SettingsTab::addRoot);
    connect(removeBtn, &QPushButton::clicked, this, &SettingsTab::removeRoot);
    connect(saveBtn, &QPushButton::clicked, this, &SettingsTab::save);

    loadFromSettings();
}

void SettingsTab::loadFromSettings()
{
    const QStringList roots = m_settings.scanRoots();
    const QStringList trusted = m_settings.trustedRoots();
    m_rootsList->clear();
    for (const QString& root : roots) {
        auto* item = new QListWidgetItem(root, m_rootsList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(trusted.contains(root) ? Qt::Checked : Qt::Unchecked);
        item->setData(TrustedRole, trusted.contains(root));
    }
    m_depthSpin->setValue(m_settings.scanMaxDepth());
}

void SettingsTab::addRoot()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select folder to scan"));
    if (dir.isEmpty())
        return;
    // Avoid duplicates: the same folder must not appear twice (case-insensitive
    // on Windows, matching TrustPolicy's normalization).
    for (int i = 0; i < m_rootsList->count(); ++i) {
        if (QDir::cleanPath(m_rootsList->item(i)->text()).compare(
                QDir::cleanPath(dir), Qt::CaseInsensitive) == 0) {
            QMessageBox::information(this, QStringLiteral("Add folder"),
                                     QStringLiteral("This folder is already in the list."));
            return;
        }
    }
    auto* item = new QListWidgetItem(dir, m_rootsList);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
    item->setData(TrustedRole, false);
}

void SettingsTab::removeRoot()
{
    const int row = m_rootsList->currentRow();
    if (row < 0)
        return;
    delete m_rootsList->takeItem(row);
}

void SettingsTab::save()
{
    QStringList roots;
    QStringList trusted;
    for (int i = 0; i < m_rootsList->count(); ++i) {
        QListWidgetItem* item = m_rootsList->item(i);
        const QString path = item->text();
        if (path.isEmpty())
            continue;
        roots.append(path);
        if (item->checkState() == Qt::Checked)
            trusted.append(path);
    }
    m_settings.setScanRoots(roots);
    m_settings.setTrustedRoots(trusted);
    m_settings.setScanMaxDepth(m_depthSpin->value());
    emit saveAndScan();
}