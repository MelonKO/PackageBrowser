#pragma once

#include "settings/AppSettings.h"

#include <QWidget>

class QListWidget;
class QGroupBox;
class QSpinBox;
class QPushButton;

// Settings screen: which project folders to scan, which of them are trusted
// (so their manager tools may run), and the scan depth. Changes are persisted
// immediately through AppSettings; a rescan is triggered by the user or
// automatically on change. See docs/UI на Qt.md.
class SettingsTab : public QWidget {
    Q_OBJECT

public:
    explicit SettingsTab(AppSettings& settings, QWidget* parent = nullptr);

signals:
    // Ask the catalog to run a scan with the updated settings.
    void scanRequested();

private:
    AppSettings& m_settings;
    QListWidget* m_scanRootsList = nullptr;
    QListWidget* m_trustedRootsList = nullptr;
    QSpinBox* m_depthSpin = nullptr;
};