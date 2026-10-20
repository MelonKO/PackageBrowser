#include "settings/AppSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSettings>

namespace {
const QString kFileName = QStringLiteral("package-browser.ini");
} // namespace

class AppSettings::Impl {
public:
    explicit Impl(const QString& iniPath)
        : settings(iniPath, QSettings::IniFormat)
    {
        settings.beginGroup(QStringLiteral("scan"));
    }

    ~Impl() { settings.endGroup(); }

    QSettings settings;
};

AppSettings::AppSettings(const QString& iniPath)
    : d(new Impl(iniPath.isEmpty() ? defaultIniPath() : iniPath))
{
}

QString AppSettings::defaultIniPath()
{
    // Prefer the directory next to the executable so settings travel with the
    // app; fall back to a user-writable config location when that directory is
    // not writable (e.g. installed under Program Files).
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString exePath = QDir(exeDir).filePath(kFileName);
    if (QFileInfo(exeDir).isWritable())
        return exePath;
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
        .filePath(kFileName);
}

AppSettings::~AppSettings()
{
    delete d;
}

QStringList AppSettings::scanRoots() const
{
    return d->settings.value(QStringLiteral("roots")).toStringList();
}

void AppSettings::setScanRoots(const QStringList& roots)
{
    d->settings.setValue(QStringLiteral("roots"), roots);
}

QStringList AppSettings::trustedRoots() const
{
    return d->settings.value(QStringLiteral("trusted")).toStringList();
}

void AppSettings::setTrustedRoots(const QStringList& roots)
{
    d->settings.setValue(QStringLiteral("trusted"), roots);
}

int AppSettings::scanMaxDepth() const
{
    return d->settings.value(QStringLiteral("maxDepth"), 3).toInt();
}

void AppSettings::setScanMaxDepth(int depth)
{
    d->settings.setValue(QStringLiteral("maxDepth"), depth);
}

QStringList AppSettings::disabledPluginIds() const
{
    return d->settings.value(QStringLiteral("disabledPlugins")).toStringList();
}

void AppSettings::setDisabledPluginIds(const QStringList& ids)
{
    d->settings.setValue(QStringLiteral("disabledPlugins"), ids);
}