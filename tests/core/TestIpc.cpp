#include "core/ipc/ApplicationRequestDispatcher.h"
#include "core/ipc/IpcProtocol.h"
#include "core/ipc/SingleInstanceClient.h"
#include "core/ipc/SingleInstanceServer.h"
#include "core/ipc/StartupCoordinator.h"
#include <QCoreApplication>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QSignalSpy>
#include <QStandardPaths>
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

  void testUICompletionToken() {
    ApplicationRequestDispatcher dispatcher;

    QSignalSpy spyProcess(
        &dispatcher, &ApplicationRequestDispatcher::processAddedLinesRequested);

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
    QList<QVariant> args = spyProcess.takeFirst();
    QCOMPARE(args.at(0).toStringList()[0], QString("A"));
    QString tokenIdA = args.at(1).toString();

    // Dispatch B while A is still "processing"
    QCOMPARE(dispatcher.dispatch(reqB), IpcProtocol::ResponseStatus::Accepted);

    // Ensure B is NOT popped yet
    QCOMPARE(spyProcess.count(), 0);

    // Explicitly complete with an UNRELATED token
    QMetaObject::invokeMethod(&dispatcher, "completeCurrentProcessing",
                              Q_ARG(QString, "unrelated-token-id"));

    // Ensure B is STILL NOT popped
    QCOMPARE(spyProcess.count(), 0);

    // Now explicitly complete A with the CORRECT token
    QMetaObject::invokeMethod(&dispatcher, "completeCurrentProcessing",
                              Q_ARG(QString, tokenIdA));

    // Now B should be popped deterministically
    QCOMPARE(spyProcess.count(), 1);
    QCOMPARE(spyProcess.takeFirst().at(0).toStringList()[0], QString("B"));
  }
  void testQueueSerialization() {
    ApplicationRequestDispatcher dispatcher;

    QSignalSpy spyProcess(
        &dispatcher, &ApplicationRequestDispatcher::processAddedLinesRequested);

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
    QList<QVariant> args = spyProcess.takeFirst();
    QCOMPARE(args.at(0).toStringList()[0], QString("A"));
    QString tokenIdA = args.at(1).toString();

    // Dispatch B while A is still "processing"
    QCOMPARE(dispatcher.dispatch(reqB), IpcProtocol::ResponseStatus::Accepted);

    // Ensure B is NOT popped yet
    QCOMPARE(spyProcess.count(), 0);

    // Unrelated completion
    QMetaObject::invokeMethod(&dispatcher, "completeCurrentProcessing",
                              Q_ARG(QString, "unrelated-token"));

    // Ensure B is NOT popped yet
    QCOMPARE(spyProcess.count(), 0);

    // Explicitly complete A
    QMetaObject::invokeMethod(&dispatcher, "completeCurrentProcessing",
                              Q_ARG(QString, tokenIdA));

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

  void testIdempotencyAmbiguousPayload() {
    ApplicationRequestDispatcher dispatcher;

    IpcProtocol::Request req1;
    req1.requestId = "req-ambig-1";
    req1.type = IpcProtocol::RequestType::AddInputs;
    req1.payload = {"ab", "c"};

    QCOMPARE(dispatcher.dispatch(req1), IpcProtocol::ResponseStatus::Accepted);

    IpcProtocol::Request req2;
    req2.requestId = "req-ambig-2";
    req2.type = IpcProtocol::RequestType::AddInputs;
    req2.payload = {"a", "bc"};

    QCOMPARE(
        dispatcher.dispatch(req2),
        IpcProtocol::ResponseStatus::Accepted); // Should NOT be a collision,
                                                // since they have different
                                                // IDs. Wait, collision is only
                                                // when they have the SAME ID!
    // But wait! If they have the SAME ID and DIFFERENT PAYLOAD, it should
    // report a collision!

    IpcProtocol::Request req1_duplicate;
    req1_duplicate.requestId = "req-ambig-1";
    req1_duplicate.type = IpcProtocol::RequestType::AddInputs;
    req1_duplicate.payload = {"a", "bc"}; // Same ID, but ambiguous payload!

    IpcProtocol::ResponseStatus s = dispatcher.dispatch(req1_duplicate);
    QCOMPARE(
        s,
        IpcProtocol::ResponseStatus::MalformedRequest); // Should be recognized
                                                        // as collision!
  }
  void testAckLostRetry() {
    QString serverName =
        "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ApplicationRequestDispatcher dispatcher;
    SingleInstanceServer server(serverName, &dispatcher);
    QVERIFY(server.tryAcquire());

    QSignalSpy spyProcess(
        &dispatcher, &ApplicationRequestDispatcher::processAddedLinesRequested);

    IpcProtocol::Request req;
    req.requestId = "req-ack-lost";
    req.type = IpcProtocol::RequestType::AddInputs;
    req.payload = {"lost-item"};

    // 1st request physically dropped mid-ACK:
    QLocalSocket socket;
    socket.connectToServer(serverName);
    QVERIFY(socket.waitForConnected(1000));

    QByteArray payload;
    QDataStream payloadOut(&payload, QIODevice::WriteOnly);
    IpcProtocol::setupStream(payloadOut);
    payloadOut << req;
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    IpcProtocol::setupStream(out);
    out << static_cast<quint32>(payload.size());
    block.append(payload);

    socket.write(block);
    QVERIFY(socket.waitForBytesWritten(1000));

    // Wait for the dispatcher to process it (server side)
    QTRY_COMPARE(spyProcess.count(), 1);

    // Disconnect abruptly without reading the ACK!
    socket.abort();

    // Simulate retry by launcher via normal client
    SingleInstanceClient client(serverName);
    ClientResult res2 = client.sendRequest(req, 1000, 1000);

    // Expected: Retry gets original success response.
    QVERIFY(res2.isSuccess());

    // Expected: Request is NOT dispatched a second time.
    QCOMPARE(spyProcess.count(), 1);
  }

  void testFramingFragmentation() {
    QString serverName =
        "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ApplicationRequestDispatcher dispatcher;
    SingleInstanceServer server(serverName, &dispatcher);
    QVERIFY(server.tryAcquire());

    QSignalSpy spyActivate(
        &dispatcher, &ApplicationRequestDispatcher::activateWindowRequested);

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
    QTRY_COMPARE_WITH_TIMEOUT(spyActivate.count(), 0, 50);

    // 2. Write next 2 bytes of the size prefix
    socket.write(block.mid(1, 2));
    QVERIFY(socket.waitForBytesWritten(1000));
    QTRY_COMPARE_WITH_TIMEOUT(spyActivate.count(), 0, 50);

    // 3. Write final byte of size prefix + half of payload
    socket.write(block.mid(3, 1 + payload.size() / 2));
    QVERIFY(socket.waitForBytesWritten(1000));
    QTRY_COMPARE_WITH_TIMEOUT(spyActivate.count(), 0, 50);

    // 4. Write the rest
    socket.write(block.mid(4 + payload.size() / 2));
    QVERIFY(socket.waitForBytesWritten(1000));

    // Now it should process!
    QTRY_COMPARE(spyActivate.count(), 1);
  }

  void testFramingResponseFragmentation() {
    QString serverName = "test-dummy-server-" +
                         QUuid::createUuid().toString(QUuid::WithoutBraces);
    QLocalServer dummyServer;
    dummyServer.listen(serverName);

    // Dummy server that writes a fragmented response back
    QObject::connect(
        &dummyServer, &QLocalServer::newConnection, &dummyServer, [&]() {
          QLocalSocket *client = dummyServer.nextPendingConnection();
          QObject::connect(
              client, &QLocalSocket::readyRead, client, [client]() {
                // Read client request and discard it
                client->readAll();

                // Build response
                IpcProtocol::Response res;
                res.requestId = "frag-res";
                res.status = IpcProtocol::ResponseStatus::Accepted;

                QByteArray payload;
                QDataStream payloadOut(&payload, QIODevice::WriteOnly);
                IpcProtocol::setupStream(payloadOut);
                payloadOut << res;

                QByteArray block;
                QDataStream out(&block, QIODevice::WriteOnly);
                IpcProtocol::setupStream(out);
                out << static_cast<quint32>(payload.size());
                block.append(payload);

                // Send length prefix byte-by-byte
                client->write(block.left(1));
                client->waitForBytesWritten(100);
                QThread::msleep(50);

                client->write(block.mid(1, 3)); // rest of length prefix
                client->waitForBytesWritten(100);
                QThread::msleep(50);

                // Send payload in chunks
                int half = payload.size() / 2;
                client->write(block.mid(4, half));
                client->waitForBytesWritten(100);
                QThread::msleep(50);

                client->write(block.mid(4 + half));
                client->waitForBytesWritten(100);

                client->disconnectFromServer();
              });
        });

    SingleInstanceClient client(serverName);
    IpcProtocol::Request req;
    req.requestId = "frag-res";
    req.type = IpcProtocol::RequestType::ActivateWindow;

    // The client should correctly wait for all the fragments to arrive and
    // return success
    ClientResult res = client.sendRequest(req, 1000, 5000);
    QVERIFY(res.isSuccess());
  }

  void testFramingTruncated() {
    QString serverName =
        "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
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

    QTRY_VERIFY(socket.state() == QLocalSocket::UnconnectedState);
  }

  void testFramingInvalidSize() {
    QString serverName =
        "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
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

    QTRY_VERIFY(socket.state() == QLocalSocket::UnconnectedState);
  }

  void testStartupCoordinatorElection() {
    QString serverName =
        "test-election-" + QUuid::createUuid().toString(QUuid::WithoutBraces);

    // In a genuine unit testing environment, testing QProcess::startDetached
    // recursively forks the test runner or primary kmagmux binary causing
    // intractable hangs and 4-second message box timeouts. We just test the
    // no-arg acquisition and lock ownership here.

    StartupCoordinator coordNoArg(serverName);
    CoordinatorResult res1 =
        coordNoArg.coordinate(QStringList() << "/dummy/path");

    QCOMPARE(res1.action, CoordinatorAction::BecomePrimary);
    QVERIFY(res1.primaryLock != nullptr);

    res1.primaryLock->unlock();
    res1.primaryLock.reset();
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

  void testFramingBadMagic() {
    QString serverName =
        "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ApplicationRequestDispatcher dispatcher;
    SingleInstanceServer server(serverName, &dispatcher);
    QVERIFY(server.tryAcquire());

    QLocalSocket socket;
    socket.connectToServer(serverName);
    QVERIFY(socket.waitForConnected(1000));

    IpcProtocol::Request req;
    req.requestId = "req-bad-magic";
    req.type = IpcProtocol::RequestType::ActivateWindow;

    QByteArray payload;
    QDataStream payloadOut(&payload, QIODevice::WriteOnly);
    IpcProtocol::setupStream(payloadOut);
    quint32 badMagic = 0xDEADBEEF;
    payloadOut << badMagic << req.version << req.requestId
               << static_cast<quint16>(req.type) << req.payload;

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    IpcProtocol::setupStream(out);
    out << static_cast<quint32>(payload.size());
    block.append(payload);

    socket.write(block);
    QVERIFY(socket.waitForBytesWritten(1000));

    // Read response manually because SingleInstanceClient returns
    // InvalidResponse if response ID mismatches. Bad magic causes reading to
    // abort before reading ID! So requestId is empty.

    quint32 expectedSize = 0;
    while (socket.waitForReadyRead(1000)) {
      if (expectedSize == 0 &&
          socket.bytesAvailable() >= static_cast<qint64>(sizeof(quint32))) {
        QDataStream in(&socket);
        IpcProtocol::setupStream(in);
        in >> expectedSize;
      }
      if (expectedSize > 0 &&
          socket.bytesAvailable() >= static_cast<qint64>(expectedSize)) {
        QByteArray frame = socket.read(expectedSize);
        QDataStream in(&frame, QIODevice::ReadOnly);
        IpcProtocol::setupStream(in);
        IpcProtocol::Response res;
        in >> res;
        // Because magic fails, `req` decoding aborted, `req.version` became 0.
        // Server responds with UnsupportedVersion, but `requestId` will be
        // empty.
        QCOMPARE(res.status, IpcProtocol::ResponseStatus::UnsupportedVersion);
        QCOMPARE(res.requestId, QString(""));
        return;
      }
    }
  }
  void testFramingUnsupportedVersion() {
    QString serverName =
        "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ApplicationRequestDispatcher dispatcher;
    SingleInstanceServer server(serverName, &dispatcher);
    QVERIFY(server.tryAcquire());

    QLocalSocket socket;
    socket.connectToServer(serverName);
    QVERIFY(socket.waitForConnected(1000));

    IpcProtocol::Request req;
    req.version = 999;
    req.requestId = "req-bad-ver";
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

    socket.write(block);
    QVERIFY(socket.waitForBytesWritten(1000));

    quint32 expectedSize = 0;
    QEventLoop loop;
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QLocalSocket::readyRead, &loop, [&]() {
      if (expectedSize == 0 &&
          socket.bytesAvailable() >= static_cast<qint64>(sizeof(quint32))) {
        QDataStream in(&socket);
        IpcProtocol::setupStream(in);
        in >> expectedSize;
      }
      if (expectedSize > 0 &&
          socket.bytesAvailable() >= static_cast<qint64>(expectedSize)) {
        QByteArray frame = socket.read(expectedSize);
        QDataStream in(&frame, QIODevice::ReadOnly);
        IpcProtocol::setupStream(in);
        IpcProtocol::Response res;
        in >> res;

        QCOMPARE(res.status, IpcProtocol::ResponseStatus::UnsupportedVersion);
        QCOMPARE(res.requestId, QString("req-bad-ver"));
        loop.quit();
      }
    });
    loop.exec();

    QVERIFY(socket.state() == QLocalSocket::UnconnectedState ||
            expectedSize > 0);
  }

  void testFramingUnknownRequestType() {
    QString serverName =
        "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ApplicationRequestDispatcher dispatcher;
    SingleInstanceServer server(serverName, &dispatcher);
    QVERIFY(server.tryAcquire());

    QLocalSocket socket;
    socket.connectToServer(serverName);
    QVERIFY(socket.waitForConnected(1000));

    IpcProtocol::Request req;
    req.version = IpcProtocol::VERSION;
    req.requestId = "req-bad-type";
    req.type = static_cast<IpcProtocol::RequestType>(999);

    QByteArray payload;
    QDataStream payloadOut(&payload, QIODevice::WriteOnly);
    IpcProtocol::setupStream(payloadOut);
    payloadOut << req;

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    IpcProtocol::setupStream(out);
    out << static_cast<quint32>(payload.size());
    block.append(payload);

    socket.write(block);
    QVERIFY(socket.waitForBytesWritten(1000));

    quint32 expectedSize = 0;
    QEventLoop loop;
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QLocalSocket::readyRead, &loop, [&]() {
      if (expectedSize == 0 &&
          socket.bytesAvailable() >= static_cast<qint64>(sizeof(quint32))) {
        QDataStream in(&socket);
        IpcProtocol::setupStream(in);
        in >> expectedSize;
      }
      if (expectedSize > 0 &&
          socket.bytesAvailable() >= static_cast<qint64>(expectedSize)) {
        QByteArray frame = socket.read(expectedSize);
        QDataStream in(&frame, QIODevice::ReadOnly);
        IpcProtocol::setupStream(in);
        IpcProtocol::Response res;
        in >> res;

        QCOMPARE(res.status, IpcProtocol::ResponseStatus::UnknownRequestType);
        QCOMPARE(res.requestId, QString("req-bad-type"));
        loop.quit();
      }
    });
    loop.exec();

    QVERIFY(socket.state() == QLocalSocket::UnconnectedState ||
            expectedSize > 0);
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
