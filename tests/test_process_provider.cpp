#include <QtTest/QtTest>

#include "plugin/PluginManifest.h"
#include "process/ProcessRunner.h"
#include "provider/ProcessProvider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

// A mock JSON-RPC provider process written to a temp script. It answers the
// four read operations the way a real vcpkg-style provider would, so ProcessProvider
// (an IProvider adapter) is exercised over a real stdio channel.
class ProcessProviderTest : public QObject {
    Q_OBJECT

private slots:
    void manifestRejectsProviderAndToolTogether();
    void detectListsToolInstances();
    void quarantinesAfterRepeatedFailures();
    void vcpkgPluginReadsProjectSpdx();

private:
    static QString script();
    static QString pythonBinary();
    static QString providerManifest();
};

// The plugin answers each request with a fixed fixture. Kept minimal and inline
// so the test has no external dependency beyond the local Python.
QString ProcessProviderTest::script()
{
    return QStringLiteral(R"PY(
import sys, json

def recv_message():
    headers = {}
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        line = line.strip()
        if not line:
            break
        k, v = line.split(b":", 1)
        headers[k.lower().strip()] = v.strip()
    body = sys.stdin.buffer.read(int(headers[b"content-length"]))
    return json.loads(body)

def send(msg):
    data = json.dumps(msg).encode()
    sys.stdout.buffer.write(b"Content-Length: %d\r\n\r\n" % len(data) + data)
    sys.stdout.buffer.flush()

while True:
    req = recv_message()
    if req is None:
        break
    method = req.get("method")
    if method == "detect":
        send({"jsonrpc": "2.0", "id": req["id"],
              "result": {"tools": [{"path": "C:/mock/vcpkg.exe", "version": "1.2.3"}]}})
    elif method == "locate":
        send({"jsonrpc": "2.0", "id": req["id"],
              "result": {"environments": [
                  {"root": "C:/mock/installed", "scope": "system"}]}})
    elif method == "list":
        send({"jsonrpc": "2.0", "id": req["id"],
              "result": {"installations": [
                  {"name": "zlib", "version": "1.3", "location": "C:/mock/lib"}]}})
    else:
        send({"jsonrpc": "2.0", "id": req["id"],
              "error": {"code": -32601, "message": "Method not found"}})
sys.exit(0)
)PY");
}

QString ProcessProviderTest::pythonBinary()
{
    // Same system-Python resolution the RPC test uses: prefer the launcher,
    // skip Store shims (0-byte) and MSYS builds.
    const QStringList names = QStringLiteral("py python3 python").split(QLatin1Char(' '));
    const QStringList avoided = {
        QStringLiteral("WindowsApps"), QStringLiteral("/mingw64/"),
        QStringLiteral("/msys64/"), QStringLiteral("\\mingw64\\"), QStringLiteral("\\msys64\\")
    };
    for (const QString& n : names) {
        const QString found = pb::findOnPath(n);
        if (found.isEmpty() || QFileInfo(found).size() == 0)
            continue;
        bool isMsys = false;
        for (const QString& a : avoided)
            if (found.contains(a, Qt::CaseInsensitive))
                isMsys = true;
        if (!isMsys)
            return found;
    }
    return QString();
}

QString ProcessProviderTest::providerManifest()
{
    return QStringLiteral(R"toml(
[plugin]
id = "mock.vcpkg"
api = 1
name = "mock-vcpkg"
ecosystem = "cpp"
capabilities = ["detect", "locate", "list"]

[provider]
exec = ["__PYTHON__", "__PLUGIN_SCRIPT__"]
)toml");
}

void ProcessProviderTest::manifestRejectsProviderAndToolTogether()
{
    QString text = providerManifest();
    text.replace(QStringLiteral("??"), QString());
    text += QStringLiteral("\n[tool]\nexecutables = [\"vcpkg\"]\n");
    QString error;
    auto manifest = PluginManifest::parse(text, QStringLiteral("mock/plugin.toml"), &error);
    QVERIFY(!manifest.has_value());
}

void ProcessProviderTest::detectListsToolInstances()
{
    const QString py = pythonBinary();
    if (py.isEmpty())
        QSKIP("no system python on PATH");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString scriptPath = dir.filePath(QStringLiteral("vcpkg_provider.py"));
    QFile f(scriptPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(script().toUtf8());
    f.close();

    QString text = providerManifest();
    // Use forward slashes so the backslashes never reach the TOML string as
    // escape sequences.
    QString forward = scriptPath;
    QString pythonForward = py;
    text.replace(QStringLiteral("__PLUGIN_SCRIPT__"),
                 forward.replace(QLatin1Char('\\'), QLatin1Char('/')));
    text.replace(QStringLiteral("__PYTHON__"),
                 pythonForward.replace(QLatin1Char('\\'), QLatin1Char('/')));
    QString error;
    auto manifest = PluginManifest::parse(text, QStringLiteral("mock/plugin.toml"), &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));

    ProcessProvider provider(*manifest);
    QVERIFY(provider.hasCapability(Capability::Detect));
    QVERIFY(provider.hasCapability(Capability::Locate));
    QVERIFY(provider.hasCapability(Capability::List));

    const auto detected = provider.detect();
    QVERIFY(detected.ok());
    QCOMPARE(detected.value.size(), 1);
    QCOMPARE(detected.value[0].path(), QStringLiteral("C:/mock/vcpkg.exe"));
    QCOMPARE(detected.value[0].version(), QStringLiteral("1.2.3"));

    const auto located = provider.locate(detected.value[0]);
    QVERIFY(located.ok());
    QCOMPARE(located.value.size(), 1);
    QCOMPARE(located.value[0].scope(), Scope::System);
    QCOMPARE(located.value[0].root(), QStringLiteral("C:/mock/installed"));

    const auto listed = provider.list(located.value[0]);
    QVERIFY(listed.ok());
    QCOMPARE(listed.value.size(), 1);
    QCOMPARE(listed.value[0].nameNorm(), QStringLiteral("zlib"));
    QCOMPARE(listed.value[0].version(), QStringLiteral("1.3"));
}

void ProcessProviderTest::quarantinesAfterRepeatedFailures()
{
    const QString py = pythonBinary();
    if (py.isEmpty())
        QSKIP("no system python on PATH");

    // A provider whose process answers every request with an EnvBroken error:
    // repeated failures must escalate Failed -> Quarantined.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString scriptPath = dir.filePath(QStringLiteral("failing_provider.py"));
    const QString failingScript = QStringLiteral(R"PY(
import sys, json

def recv_message():
    headers = {}
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        line = line.strip()
        if not line:
            break
        k, v = line.split(b":", 1)
        headers[k.lower().strip()] = v.strip()
    body = sys.stdin.buffer.read(int(headers[b"content-length"]))
    return json.loads(body)

def send(msg):
    data = json.dumps(msg).encode()
    sys.stdout.buffer.write(b"Content-Length: %d\r\n\r\n" % len(data) + data)
    sys.stdout.buffer.flush()

while True:
    req = recv_message()
    if req is None:
        break
    send({"jsonrpc": "2.0", "id": req["id"],
          "error": {"code": -32000, "message": "boom",
                    "data": {"kind": "EnvBroken"}}})
sys.exit(0)
)PY");
    QFile f(scriptPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(failingScript.toUtf8());
    f.close();

    QString forward = scriptPath;
    QString text = providerManifest();
    text.replace(QStringLiteral("__PLUGIN_SCRIPT__"),
                 forward.replace(QLatin1Char('\\'), QLatin1Char('/')));
    QString pythonForward = py;
    text.replace(QStringLiteral("__PYTHON__"),
                 pythonForward.replace(QLatin1Char('\\'), QLatin1Char('/')));
    QString error;
    auto manifest = PluginManifest::parse(text, QStringLiteral("mock/plugin.toml"), &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));

    ProcessProvider provider(*manifest);
    QCOMPARE(provider.state(), ProcessProviderState::Ready);

    // Each failing request bumps the streak. The final one crosses the
    // threshold and quarantines the provider.
    for (int i = 0; i < ProcessProvider::kQuarantineThreshold; ++i) {
        const auto detected = provider.detect();
        QVERIFY(!detected.ok());
    }
    QCOMPARE(provider.state(), ProcessProviderState::Quarantined);

    // A quarantined provider refuses further calls until re-enabled.
    const auto refused = provider.detect();
    QVERIFY(!refused.ok());

    provider.reenable();
    QCOMPARE(provider.state(), ProcessProviderState::Ready);
}

void ProcessProviderTest::vcpkgPluginReadsProjectSpdx()
{
    const QString py = pythonBinary();
    if (py.isEmpty())
        QSKIP("no system python on PATH");

    // Point the provider at the real in-repo vcpkg_provider.py.
    const QString repoPlugin = QStringLiteral("C:/Projects/C++/PackageBrowser (Multitool)/plugins/vcpkg/vcpkg_provider.py");
    if (!QFileInfo::exists(repoPlugin))
        QSKIP("vcpkg_provider.py not in repo tree");

    // Fake VCPKG_ROOT with one triplet containing a fake vcpkg.exe whose
    // `list --x-json` returns one package. The double-sided listing must:
    // 1) for scope=system, ask vcpkg.exe (JSON);
    // 2) for scope=project, read share/<pkg>/vcpkg.spdx.json (no execution).
    QTemporaryDir root;
    QVERIFY(root.isValid());

    // The .exe-based system path needs a real vcpkg.exe, which is platform and
    // environment specific, so the C++ test covers only the deterministic
    // file-based project path (manifest mode: read from SPDX, nothing executed).

    // --- project env: manifest mode via SPDX files (no execution) ---
    const QString proj = root.filePath(QStringLiteral("proj"));
    QVERIFY(QDir().mkpath(proj));
    QFile marker(QDir(proj).filePath(QStringLiteral("vcpkg.json")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("{\"dependencies\":[]}");
    marker.close();
    const QString pkgDir = proj + QStringLiteral("/vcpkg_installed/share/brotli");
    QVERIFY(QDir().mkpath(pkgDir));
    QFile spdx(QDir(pkgDir).filePath(QStringLiteral("vcpkg.spdx.json")));
    QVERIFY(spdx.open(QIODevice::WriteOnly));
    spdx.write("{\"packages\":[{\"SPDXID\":\"SPDXRef-port\",\"name\":\"brotli\","
               "\"versionInfo\":\"1.2.0\"}]}");
    spdx.close();

    QString text = providerManifest();
    QString scriptForward = repoPlugin;
    text.replace(QStringLiteral("__PLUGIN_SCRIPT__"), scriptForward.replace(QLatin1Char('\\'), QLatin1Char('/')));
    QString pythonForward = py;
    text.replace(QStringLiteral("__PYTHON__"), pythonForward.replace(QLatin1Char('\\'), QLatin1Char('/')));
    QString error;
    auto manifest = PluginManifest::parse(text, QStringLiteral("mock/plugin.toml"), &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));

    ProcessProvider provider(*manifest);

    // Project environment from a scan root: files-based, no execution.
    const auto locatedProject = provider.locateAt(proj);
    QVERIFY(locatedProject.ok());
    QCOMPARE(locatedProject.value.size(), 1);
    QCOMPARE(locatedProject.value[0].scope(), Scope::Project);
    QCOMPARE(locatedProject.value[0].root(), QDir(proj).canonicalPath());

    const auto listedProject = provider.list(locatedProject.value[0]);
    QVERIFY(listedProject.ok());
    QCOMPARE(listedProject.value.size(), 1);
    QCOMPARE(listedProject.value[0].nameNorm(), QStringLiteral("brotli"));
    QCOMPARE(listedProject.value[0].version(), QStringLiteral("1.2.0"));
}

QTEST_MAIN(ProcessProviderTest)
#include "test_process_provider.moc"