#pragma once

#include <QStringList>

// User settings (scan roots, trusted roots) kept in QSettings, separate from
// the disposable SQLite cache. They survive a cache reset.
// See docs/Хранилище и кэш.md ("Settings").
class AppSettings {
public:
    AppSettings();
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