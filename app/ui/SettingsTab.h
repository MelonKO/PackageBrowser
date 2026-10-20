#pragma once

#include "settings/AppSettings.h"

#include <QWidget>

class QListWidget;
class QSpinBox;
class QPushButton;

// Settings screen: one list of project folders to scan, each marked with a
// checkbox for whether it is trusted (so its manager tools may run; otherwise
// the folder is read from files only). Changes stay in the edit controls until
// "Save Settings" persists them to AppSettings and requests a scan; nothing is
// saved or scanned on every edit. See docs/UI на Qt.md.
class SettingsTab : public QWidget {
    Q_OBJECT

public:
    explicit SettingsTab(AppSettings& settings, QWidget* parent = nullptr);

signals:
    // Ask the catalog to run a scan with the freshly saved settings.
    void saveAndScan();

private slots:
    void addRoot();
    void removeRoot();
    void save();

private:
    // Reload the list and spin box from AppSettings.
    void loadFromSettings();

    AppSettings& m_settings;
    QListWidget* m_rootsList = nullptr;
    QSpinBox* m_depthSpin = nullptr;
};