#include "settings/AppSettings.h"

#include <QSettings>

namespace {
const QString kOrg = QStringLiteral("package-browser");
const QString kApp = QStringLiteral("package-browser");
}

class AppSettings::Impl {
public:
    QSettings settings;
};

AppSettings::AppSettings()
    : d(new Impl)
{
    d->settings.beginGroup(QStringLiteral("scan"));
}

AppSettings::~AppSettings()
{
    d->settings.endGroup();
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