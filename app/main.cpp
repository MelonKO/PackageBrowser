#include "app/ui/MainWindow.h"
#include "catalog/CatalogService.h"

#include <QApplication>

#include <QDir>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("package-browser"));
    app.setOrganizationName(QStringLiteral("package-browser"));

    CatalogService catalog;
    const QString dbPath = QDir::temp().filePath(QStringLiteral("pb_cache.db"));
    if (!catalog.initialize(dbPath))
        return 1;

    MainWindow window(&catalog);
    window.show();

    // Scan in the background after the window is visible (stale-while-revalidate).
    catalog.rescan();

    return app.exec();
}