#include "core/ipc/StartupCoordinator.h"
#include <QStandardPaths>
#include <QDir>
#include <QLockFile>
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

    QByteArray payload;
    QDataStream payloadOut(&payload, QIODevice::WriteOnly);
    IpcProtocol::setupStream(payloadOut);
    payloadOut << req1;

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    IpcProtocol::setupStream(out);
    out << static_cast<quint32>(payload.size());
    block.append(payload);

    QDataStream in(&block, QIODevice::ReadOnly);
    IpcProtocol::setupStream(in);
    quint32 size;
    in >> size;
    QCOMPARE(size, static_cast<quint32>(payload.size()));
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

    QByteArray payload;
    QDataStream payloadOut(&payload, QIODevice::WriteOnly);
    IpcProtocol::setupStream(payloadOut);
    payloadOut << res1;

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    IpcProtocol::setupStream(out);
    out << static_cast<quint32>(payload.size());
    block.append(payload);

    QDataStream in(&block, QIODevice::ReadOnly);
    IpcProtocol::setupStream(in);
    quint32 size;
    in >> size;
    QCOMPARE(size, static_cast<quint32>(payload.size()));
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

  void testQueueSerialization() {
    ApplicationRequestDispatcher dispatcher;

    QSignalSpy spyProcess(&dispatcher, &ApplicationRequestDispatcher::processAddedLinesRequested);

    IpcProtocol::Request reqA;
    reqA.requestId = "req-A";
    reqA.type = IpcProtocol::RequestType::AddInputs;
    reqA.payload = {"A"};

    IpcProtocol::Request reqB;
    reqB.requestId = "req-B";
    reqB.type = IpcProtocol::RequestType::AddInputs;
    reqB.payload = {"B"};

    // Dispatch A
    QCOMPARE(dispatcher.dispatch(reqA), IpcProtocol::ResponseStatus::Accepted);

    // Ensure A is popped deterministically
    QCOMPARE(spyProcess.count(), 1);
    QCOMPARE(spyProcess.takeFirst().at(0).toStringList()[0], QString("A"));

    // Dispatch B while A is still "processing"
    QCOMPARE(dispatcher.dispatch(reqB), IpcProtocol::ResponseStatus::Accepted);

    // Ensure B is NOT popped yet
    QCOMPARE(spyProcess.count(), 0);

    // Explicitly complete A
    dispatcher.property("completeCurrentProcessing");
    QMetaObject::invokeMethod(&dispatcher, "completeCurrentProcessing");

    // Now B should be popped deterministically
    QCOMPARE(spyProcess.count(), 1);
    QCOMPARE(spyProcess.takeFirst().at(0).toStringList()[0], QString("B"));
  }

  void testIdempotencyCollisions() {
    ApplicationRequestDispatcher dispatcher;

    IpcProtocol::Request req;
    req.requestId = "req-shared";
    req.type = IpcProtocol::RequestType::ActivateWindow;

    QCOMPARE(dispatcher.dispatch(req), IpcProtocol::ResponseStatus::Accepted);

    // Same ID but different type (Collision)
    IpcProtocol::Request reqCollision1;
    reqCollision1.requestId = "req-shared";
    reqCollision1.type = IpcProtocol::RequestType::AddInputs;

    IpcProtocol::ResponseStatus s1 = dispatcher.dispatch(reqCollision1);
    QCOMPARE(s1, IpcProtocol::ResponseStatus::MalformedRequest);

    // Different payload (Collision)
    IpcProtocol::Request reqCollision2;
    reqCollision2.requestId = "req-shared";
    reqCollision2.type = IpcProtocol::RequestType::ActivateWindow;
    reqCollision2.payload = {"Collision"};

    IpcProtocol::ResponseStatus s2 = dispatcher.dispatch(reqCollision2);
    QCOMPARE(s2, IpcProtocol::ResponseStatus::MalformedRequest);
  }

  void testAckLostRetry() {
    QString serverName =
        "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ApplicationRequestDispatcher dispatcher;
    SingleInstanceServer server(serverName, &dispatcher);
    QVERIFY(server.tryAcquire());

    QSignalSpy spyProcess(&dispatcher, &ApplicationRequestDispatcher::processAddedLinesRequested);

    IpcProtocol::Request req;
    req.requestId = "req-ack-lost";
    req.type = IpcProtocol::RequestType::AddInputs;
    req.payload = {"lost-item"};

    // 1st request accepted by dispatcher, but let's assume the ACK got lost.
    // Instead of actually losing the ACK (which requires mocking socket), we just assert it succeeds
    SingleInstanceClient client(serverName);
    ClientResult res1 = client.sendRequest(req, 1000, 1000);
    QVERIFY(res1.isSuccess());

    QCOMPARE(spyProcess.count(), 1); // Dispatched once

    // Simulate retry by launcher with identical Request ID and Payload
    ClientResult res2 = client.sendRequest(req, 1000, 1000);
    QVERIFY(res2.isSuccess()); // Cached response works

    QCOMPARE(spyProcess.count(), 1); // STILL ONE! (Idempotency deduplicated it)
  }

  void testFramingFragmentation() {
    QString serverName = "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ApplicationRequestDispatcher dispatcher;
    SingleInstanceServer server(serverName, &dispatcher);
    QVERIFY(server.tryAcquire());

    QSignalSpy spyActivate(&dispatcher, &ApplicationRequestDispatcher::activateWindowRequested);

    QLocalSocket socket;
    socket.connectToServer(serverName);
    QVERIFY(socket.waitForConnected(1000));

    IpcProtocol::Request req;
    req.requestId = "req-frag";
    req.type = IpcProtocol::RequestType::ActivateWindow;

    QByteArray payload;
    QDataStream payloadOut(&payload, QIODevice::WriteOnly);
    IpcProtocol::setupStream(payloadOut);
    payloadOut << req;

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    IpcProtocol::setupStream(out);
    out << static_cast<quint32>(payload.size());
    block.append(payload);

    // 1. Write just 1 byte of the 4-byte size prefix
    socket.write(block.left(1));
    QVERIFY(socket.waitForBytesWritten(1000));
    QTest::qWait(100);
    QCOMPARE(spyActivate.count(), 0);

    // 2. Write next 2 bytes of the size prefix
    socket.write(block.mid(1, 2));
    QVERIFY(socket.waitForBytesWritten(1000));
    QTest::qWait(100);
    QCOMPARE(spyActivate.count(), 0);

    // 3. Write final byte of size prefix + half of payload
    socket.write(block.mid(3, 1 + payload.size() / 2));
    QVERIFY(socket.waitForBytesWritten(1000));
    QTest::qWait(100);
    QCOMPARE(spyActivate.count(), 0);

    // 4. Write the rest
    socket.write(block.mid(4 + payload.size() / 2));
    QVERIFY(socket.waitForBytesWritten(1000));

    // Now it should process!
    QTest::qWait(500);
    QCOMPARE(spyActivate.count(), 1);
  }

  void testFramingTruncated() {
    QString serverName = "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ApplicationRequestDispatcher dispatcher;
    SingleInstanceServer server(serverName, &dispatcher);
    QVERIFY(server.tryAcquire());

    QLocalSocket socket;
    socket.connectToServer(serverName);
    QVERIFY(socket.waitForConnected(1000));

    IpcProtocol::Request req;
    req.requestId = "req-trunc";
    req.type = IpcProtocol::RequestType::ActivateWindow;

    QByteArray payload;
    QDataStream payloadOut(&payload, QIODevice::WriteOnly);
    IpcProtocol::setupStream(payloadOut);
    payloadOut << req;

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    IpcProtocol::setupStream(out);
    out << static_cast<quint32>(payload.size());
    block.append(payload);

    // Write size but only half the payload, then disconnect
    socket.write(block.left(4 + payload.size() / 2));
    QVERIFY(socket.waitForBytesWritten(1000));
    socket.disconnectFromServer();

    QTest::qWait(500); // Allow server to clean it up (or timeout)
  }

  void testFramingInvalidSize() {
    QString serverName = "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ApplicationRequestDispatcher dispatcher;
    SingleInstanceServer server(serverName, &dispatcher);
    QVERIFY(server.tryAcquire());

    QLocalSocket socket;
    socket.connectToServer(serverName);
    QVERIFY(socket.waitForConnected(1000));

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    IpcProtocol::setupStream(out);
    out << static_cast<quint32>(15 * 1024 * 1024); // 15MB is over 10MB limit

    socket.write(block);
    QVERIFY(socket.waitForBytesWritten(1000));

    QTest::qWait(200);
    // The server should immediately disconnect upon invalid size
    QVERIFY(socket.state() == QLocalSocket::UnconnectedState || socket.waitForDisconnected(1000));
  }

  void testStartupCoordinatorElection() {
      // Create a unique server name
      QString serverName = "test-election-" + QUuid::createUuid().toString(QUuid::WithoutBraces);

      // Simulate no-arg launcher
      StartupCoordinator coordNoArg(serverName);
      CoordinatorResult res1 = coordNoArg.coordinate(QStringList() << "/dummy/path");

      // Should become primary because it has no arguments and lock is free
      QCOMPARE(res1.action, CoordinatorAction::BecomePrimary);
      QVERIFY(res1.primaryLock != nullptr);

      // Instead of running the full action-bearing launcher which triggers a blocking UI dialog when it fails to connect,
      // we can just test that the lock is actually acquired.
      QLockFile *testLock = new QLockFile(QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)).filePath(serverName + ".lock"));
      testLock->setStaleLockTime(0);
      QVERIFY(!testLock->tryLock(0)); // Should fail because res1 holds it!
      delete testLock;

      res1.primaryLock->unlock();
      delete res1.primaryLock;
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
