#pragma once

#include <QString>
#include <QStringList>

// User settings (scan roots, trusted roots) kept in an INI file next to the
// application, separate from the disposable SQLite cache. They survive a cache
// reset. Unlike QSettings' default registry storage on Windows, this is a plain
// portable file (INI format), addressed explicitly.
// See docs/Хранилище и кэш.md ("Settings").
class AppSettings {
public:
    // iniPath: full path to the settings file. When empty the file is placed
    // next to the application binary as <appdir>/package-browser.ini.
    explicit AppSettings(const QString& iniPath = QString());
    ~AppSettings();

    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;

    QStringList scanRoots() const;
    void setScanRoots(const QStringList& roots);

    QStringList trustedRoots() const;
    void setTrustedRoots(const QStringList& roots);

    int scanMaxDepth() const;
    void setScanMaxDepth(int depth);

    // Plugin ids a user has disabled. Disabled plugins are not loaded and their
    // environments are skipped; data stays in the cache, marked out of date.
    QStringList disabledPluginIds() const;
    void setDisabledPluginIds(const QStringList& ids);

private:
    class Impl;
    Impl* d;
};