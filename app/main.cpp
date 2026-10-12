#include "app/ui/MainWindow.h"
#include "catalog/CatalogService.h"

#include <QApplication>
#include <QDir>
#include <QTimer>

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

    // Run the scan once the event loop starts so the window paints the cached
    // data first (stale-while-revalidate); progress arrives via scanProgress.
    QTimer::singleShot(0, &catalog, [&catalog]() { catalog.rescan(); });

    return app.exec();
}