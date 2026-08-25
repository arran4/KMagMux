#include "core/ipc/ApplicationRequestDispatcher.h"
#include "core/ipc/IpcProtocol.h"
#include "core/ipc/SingleInstanceClient.h"
#include "core/ipc/SingleInstanceServer.h"
#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QUuid>

class TestIpc : public QObject {
  Q_OBJECT

private slots:
  void testRequestEncodeDecode() {
    IpcProtocol::Request req1;
    req1.requestId = "test-123";
    req1.type = IpcProtocol::RequestType::AddInputs;
    req1.payload = {"input1", "input2"};

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out << req1;

    QDataStream in(&block, QIODevice::ReadOnly);
    IpcProtocol::Request req2;
    in >> req2;

    QCOMPARE(req2.version, IpcProtocol::VERSION);
    QCOMPARE(req2.requestId, QString("test-123"));
    QCOMPARE(req2.type, IpcProtocol::RequestType::AddInputs);
    QCOMPARE(req2.payload.size(), 2);
    QCOMPARE(req2.payload[0], QString("input1"));
  }

  void testResponseEncodeDecode() {
    IpcProtocol::Response res1;
    res1.requestId = "test-123";
    res1.status = IpcProtocol::ResponseStatus::Accepted;
    res1.errorMessage = "OK";

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out << res1;

    QDataStream in(&block, QIODevice::ReadOnly);
    IpcProtocol::Response res2;
    in >> res2;

    QCOMPARE(res2.version, IpcProtocol::VERSION);
    QCOMPARE(res2.requestId, QString("test-123"));
    QCOMPARE(res2.status, IpcProtocol::ResponseStatus::Accepted);
    QCOMPARE(res2.errorMessage, QString("OK"));
  }

  void testClientServerSuccess() {
    QString serverName =
        "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ApplicationRequestDispatcher dispatcher;
    SingleInstanceServer server(serverName, &dispatcher);
    QVERIFY(server.tryAcquire());

    QSignalSpy spyActivate(
        &dispatcher, &ApplicationRequestDispatcher::activateWindowRequested);
    QSignalSpy spyProcess(
        &dispatcher, &ApplicationRequestDispatcher::processAddedLinesRequested);

    SingleInstanceClient client(serverName);
    IpcProtocol::Request req;
    req.requestId = "req-1";
    req.type = IpcProtocol::RequestType::AddInputs;
    req.payload = {"magnet:?xt=urn:btih:123"};

    ClientResult res = client.sendRequest(req, 1000, 1000);

    QVERIFY(res.isSuccess());
    QCOMPARE(res.code, ClientResultCode::Accepted);

    QCOMPARE(spyActivate.count(), 1);

    // Wait for processNext
    QTest::qWait(100);
    QCOMPARE(spyProcess.count(), 1);
    QList<QVariant> args = spyProcess.takeFirst();
    QStringList payload = args.at(0).toStringList();
    QCOMPARE(payload.size(), 1);
    QCOMPARE(payload[0], QString("magnet:?xt=urn:btih:123"));
  }

  void testClientServerNoServer() {
    QString serverName = "non-existent-server";
    SingleInstanceClient client(serverName);
    IpcProtocol::Request req;
    req.requestId = "req-1";
    req.type = IpcProtocol::RequestType::AddInputs;

    ClientResult res = client.sendRequest(req, 100, 100);
    QVERIFY(!res.isSuccess());
    QCOMPARE(res.code, ClientResultCode::ConnectFailed);
  }

  void testIdempotency() {
    QString serverName =
        "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ApplicationRequestDispatcher dispatcher;
    SingleInstanceServer server(serverName, &dispatcher);
    QVERIFY(server.tryAcquire());

    QSignalSpy spyActivate(
        &dispatcher, &ApplicationRequestDispatcher::activateWindowRequested);

    SingleInstanceClient client(serverName);
    IpcProtocol::Request req;
    req.requestId = "req-1";
    req.type = IpcProtocol::RequestType::ActivateWindow;

    ClientResult res1 = client.sendRequest(req, 1000, 1000);
    QVERIFY(res1.isSuccess());
    QCOMPARE(spyActivate.count(), 1);

    // Send the exact same request again
    ClientResult res2 = client.sendRequest(req, 1000, 1000);
    QVERIFY(res2.isSuccess());

    // Wait just in case
    QTest::qWait(100);

    // Still 1! Deduplicated.
    QCOMPARE(spyActivate.count(), 1);
  }

  void testClientServerWriteNoAck() {
    QString serverName =
        "test-no-ack-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QLocalServer dummyServer;
    dummyServer.listen(serverName);

    SingleInstanceClient client(serverName);
    IpcProtocol::Request req;
    req.requestId = "req-1";
    req.type = IpcProtocol::RequestType::ActivateWindow;

    // The client connects and writes successfully, but the dummyServer won't
    // ACK
    ClientResult res = client.sendRequest(req, 100, 100);
    QVERIFY(!res.isSuccess());
    QCOMPARE(res.code, ClientResultCode::ResponseTimeout);
  }

  void testProtocolMismatch() {
    IpcProtocol::Request req;
    req.version = 999;
    req.requestId = "req-bad";
    req.type = IpcProtocol::RequestType::ActivateWindow;
    QVERIFY(!req.isValid());
  }
};

QTEST_MAIN(TestIpc)
#include "TestIpc.moc"
