#include <QtTest/QtTest>

#include "process/ProcessRunner.h"
#include "rpc/ContentLengthFraming.h"
#include "rpc/JsonRpcClient.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>

using namespace pb::rpc;

namespace {

// A tiny JSON-RPC peer implemented as a helper process for the client tests.
// It answers requests in a loop and only exits when stdin closes, so a live
// child is what the client talks to. Written to a temp script so the tests
// cover the real stdio channel without any external dependency.
const char* kPeerProgram = R"PY(
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

mode = sys.argv[1].lower()
while True:
    req = recv_message()
    if req is None:
        break
    if mode == "echo":
        send({"jsonrpc": "2.0", "id": req["id"], "result": {"echo": req["method"]}})
    elif mode == "error":
        send({"jsonrpc": "2.0", "id": req["id"],
              "error": {"code": -32000, "message": "boom", "data": {"kind": "EnvBroken"}}})
    elif mode == "notif":
        send({"jsonrpc": "2.0", "method": "progress", "params": {"done": 1, "total": 2}})
        send({"jsonrpc": "2.0", "id": req["id"], "result": {"ok": True}})
    elif mode == "hang":
        import time; time.sleep(60)
    elif mode == "garbage":
        # An explicitly malformed frame (negative Content-Length): the decoder
        # must reject it on arrival rather than wait forever for a delimiter.
        sys.stdout.buffer.write(b"Content-Length: -1\r\n\r\nthis is not a valid frame")
        sys.stdout.buffer.flush()
        import time; time.sleep(60)
sys.exit(0)
)PY";

QString writePeer(const QString& dir, const QString& mode)
{
    const QString path = dir + QStringLiteral("/peer_") + mode + QStringLiteral(".py");
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(kPeerProgram);
    f.close();
    return path;
}

QString pythonBinary()
{
    // Use the same system Python the app itself drives (via pip), never a MSYS
    // build. The Windows Store aliases (WindowsApps) are 0-byte reparse-point
    // shims QProcess cannot spawn, and mingw64/msys64 pythons carry a MSYS
    // runtime that misbehaves as a GUI-app child over stdio. So prefer the
    // Python launcher, then real installs, skipping all of the above.
    const QStringList names = QStringLiteral("py python3 python").split(QLatin1Char(' '));
    const QStringList avoided = {
        QStringLiteral("WindowsApps"),
        QStringLiteral("/mingw64/"),
        QStringLiteral("/msys64/"),
        QStringLiteral("\\mingw64\\"),
        QStringLiteral("\\msys64\\")
    };
    for (const QString& n : names) {
        const QString found = pb::findOnPath(n);
        if (found.isEmpty())
            continue;
        if (QFileInfo(found).size() == 0)
            continue; // zero-byte Store shim
        bool isMsys = false;
        for (const QString& a : avoided)
            if (found.contains(a, Qt::CaseInsensitive))
                isMsys = true;
        if (isMsys)
            continue;
        return found;
    }
    return QString();
}

} // namespace

class RpcTest : public QObject {
    Q_OBJECT

private slots:
    void framesOneMessage();
    void decodesFragmentedAndBatched();
    void clientRoundTripsEcho();
    void clientDeliversError();
    void clientDeliversNotifications();
    void clientTimesOutHangingPlugin();
    void clientFailsOnGarbage();
};

void RpcTest::framesOneMessage()
{
    const QByteArray payload = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"list\"}";
    const QByteArray framed = frameMessage(payload);
    QVERIFY(framed.startsWith("Content-Length: "));
    QVERIFY(framed.endsWith("\r\n\r\n" + payload));
}

void RpcTest::decodesFragmentedAndBatched()
{
    const QByteArray p1 = "{\"a\":1}";
    const QByteArray p2 = "{\"b\":2}";
    const QByteArray framed = frameMessage(p1) + frameMessage(p2);

    FrameDecoder decoder;
    // Feed byte-by-byte; nothing decodes until the last byte of the first frame.
    QList<QByteArray> out;
    const QByteArray firstHalf = framed.left(framed.size() / 2);
    const QByteArray secondHalf = framed.mid(firstHalf.size());
    out += decoder.push(firstHalf.mid(0, 1));
    for (int i = 1; i < firstHalf.size(); ++i)
        out += decoder.push(firstHalf.mid(i, 1));
    out += decoder.push(secondHalf);

    QVERIFY(!decoder.malformed());
    QCOMPARE(out.size(), 2);
    QCOMPARE(out[0], p1);
    QCOMPARE(out[1], p2);
}

void RpcTest::clientRoundTripsEcho()
{
    const QString py = pythonBinary();
    if (py.isEmpty())
        QSKIP("no python on PATH");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString peer = writePeer(dir.path(), QStringLiteral("echo"));

    JsonRpcClient client;
    QVERIFY(client.start(py, { peer, QStringLiteral("echo") }));

    QJsonObject params;
    params.insert(QStringLiteral("n"), 42);
    bool finished = false;
    QJsonObject gotResult;
    QVERIFY(client.request(params, QStringLiteral("list"),
                           [&](const QJsonObject& result, const QJsonObject& error) {
                               gotResult = result;
                               Q_UNUSED(error);
                               finished = true;
                           }));

    // Run the event loop until the response arrives.
    QElapsedTimer timer;
    timer.start();
    while (!finished && timer.elapsed() < 2000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QVERIFY2(finished, "no response within timeout");
    QCOMPARE(gotResult.value(QStringLiteral("echo")).toString(), QStringLiteral("list"));
    client.stop();
}

void RpcTest::clientDeliversError()
{
    const QString py = pythonBinary();
    if (py.isEmpty())
        QSKIP("no python on PATH");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString peer = writePeer(dir.path(), QStringLiteral("error"));

    JsonRpcClient client;
    QVERIFY(client.start(py, { peer, QStringLiteral("error") }));

    bool finished = false;
    QJsonObject gotError;
    client.request({}, QStringLiteral("locate"),
                   [&](const QJsonObject&, const QJsonObject& error) {
                       gotError = error;
                       finished = true;
                   });

    QElapsedTimer timer;
    timer.start();
    while (!finished && timer.elapsed() < 2000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QVERIFY2(finished, "no response within timeout");
    QCOMPARE(gotError.value(QStringLiteral("code")).toInt(), -32000);
    client.stop();
}

void RpcTest::clientDeliversNotifications()
{
    const QString py = pythonBinary();
    if (py.isEmpty())
        QSKIP("no python on PATH");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString peer = writePeer(dir.path(), QStringLiteral("notif"));

    JsonRpcClient client;
    QVERIFY(client.start(py, { peer, QStringLiteral("notif") }));

    int notifCount = 0;
    QJsonObject gotNotif;
    client.setNotificationHandler([&](const QJsonObject& params) {
        gotNotif = params;
        ++notifCount;
    });

    bool finished = false;
    client.request({}, QStringLiteral("list"),
                   [&](const QJsonObject&, const QJsonObject&) { finished = true; });

    QElapsedTimer timer;
    timer.start();
    while ((!finished || notifCount == 0) && timer.elapsed() < 2000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QVERIFY2(finished && notifCount >= 1, "no notification or response");
    QCOMPARE(gotNotif.value(QStringLiteral("done")).toInt(), 1);
    client.stop();
}

void RpcTest::clientTimesOutHangingPlugin()
{
    const QString py = pythonBinary();
    if (py.isEmpty())
        QSKIP("no python on PATH");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString peer = writePeer(dir.path(), QStringLiteral("hang"));

    JsonRpcClient client;
    QVERIFY(client.start(py, { peer, QStringLiteral("hang") }));

    bool timedOut = false;
    client.request({}, QStringLiteral("list"),
                   [&](const QJsonObject&, const QJsonObject&) { timedOut = true; },
                   /*timeoutMs*/ 200);

    QElapsedTimer timer;
    timer.start();
    while (!timedOut && timer.elapsed() < 3000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QVERIFY2(timedOut, "hanging plugin was not timed out");
    client.stop();
}

void RpcTest::clientFailsOnGarbage()
{
    const QString py = pythonBinary();
    if (py.isEmpty())
        QSKIP("no python on PATH");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString peer = writePeer(dir.path(), QStringLiteral("garbage"));

    JsonRpcClient client;
    QVERIFY(client.start(py, { peer, QStringLiteral("garbage") }));

    bool failed = false;
    client.request({}, QStringLiteral("list"),
                   [&](const QJsonObject&, const QJsonObject&) { failed = true; });

    QElapsedTimer timer;
    timer.start();
    while (!failed && timer.elapsed() < 3000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QVERIFY2(failed, "garbage on stdout did not fail the channel");
    QCOMPARE(client.lastError(), RpcError::ParseFailed);
    client.stop();
}

QTEST_MAIN(RpcTest)
#include "test_rpc.moc"