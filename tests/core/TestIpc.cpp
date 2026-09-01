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

class MockStartupSystem : public StartupSystemInterface {
public:
  int spawnCount = 0;
  QString spawnedProgram;
  QStringList spawnedArgs;

  int errorCount = 0;
  int recoveryCount = 0;

  bool recoveryResult = false;
  bool spawnResult = true;
  ClientResultCode requestResultCode = ClientResultCode::Accepted;

  bool spawnDetached(const QString &program, const QStringList &args) override {
    spawnCount++;
    spawnedProgram = program;
    spawnedArgs = args;
    return spawnResult;
  }

  int sendRequestCount = 0;
  IpcProtocol::Request lastSentRequest;
  QString lastSentId;
  QString lastServerName;

  int lastConnectTimeout = 0;
  int lastResponseTimeout = 0;
  ClientResult sendClientRequest(const QString &serverName,
                                 const IpcProtocol::Request &req,
                                 int connectTimeout,
                                 int responseTimeout) override {
    sendRequestCount++;
    lastServerName = serverName;
    lastConnectTimeout = connectTimeout;
    lastResponseTimeout = responseTimeout;
    lastSentRequest = req;
    lastSentId = req.requestId;
    return {requestResultCode, ""};
  }

  void msleep(int) override {}

  void showErrorMessage(const QString &) override { errorCount++; }

  bool showRecoveryPrompt(const QString &) override {
    recoveryCount++;
    return recoveryResult;
  }
};

class TransientMockSystem : public MockStartupSystem {
public:
  ClientResult sendClientRequest(const QString &,
                                 const IpcProtocol::Request &req, int,
                                 int) override {
    sendRequestCount++;
    if (lastSentId.isEmpty())
      lastSentId = req.requestId;
    if (lastSentId != req.requestId) {
      return {ClientResultCode::RequestRejected, "ID mismatch"};
    }

    if (sendRequestCount <= 2)
      return {ClientResultCode::ConnectFailed, ""};
    return {ClientResultCode::Accepted, ""};
  }
};

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

    // Verify ShowWindow
    req1.type = IpcProtocol::RequestType::ShowWindow;
    payload.clear();
    block.clear();
    payloadOut.device()->seek(0);
    out.device()->seek(0);
    payloadOut << req1;
    out << static_cast<quint32>(payload.size());
    block.append(payload);
    QDataStream inShow(&block, QIODevice::ReadOnly);
    IpcProtocol::setupStream(inShow);
    inShow >> size >> req2;
    QCOMPARE(req2.type, IpcProtocol::RequestType::ShowWindow);

    // Verify ToggleWindow
    req1.type = IpcProtocol::RequestType::ToggleWindow;
    payload.clear();
    block.clear();
    payloadOut.device()->seek(0);
    out.device()->seek(0);
    payloadOut << req1;
    out << static_cast<quint32>(payload.size());
    block.append(payload);
    QDataStream inToggle(&block, QIODevice::ReadOnly);
    IpcProtocol::setupStream(inToggle);
    inToggle >> size >> req2;
    QCOMPARE(req2.type, IpcProtocol::RequestType::ToggleWindow);
  }

  void testResponseEncodeDecode() {
    IpcProtocol::Response res1;
    res1.requestId = "test-123";
    res1.rawStatus =
        static_cast<quint16>(IpcProtocol::ResponseStatus::Accepted);
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
    QCOMPARE(res2.rawStatus,
             static_cast<quint16>(IpcProtocol::ResponseStatus::Accepted));
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

    // Still 1! Deduplicated.
    QCOMPARE(spyActivate.count(), 1);
  }

  void testVisibilityRequests() {
    QString serverName =
        "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ApplicationRequestDispatcher dispatcher;
    SingleInstanceServer server(serverName, &dispatcher);
    QVERIFY(server.tryAcquire());

    QSignalSpy spyActivate(
        &dispatcher, &ApplicationRequestDispatcher::activateWindowRequested);
    QSignalSpy spyShow(&dispatcher,
                       &ApplicationRequestDispatcher::showWindowRequested);
    QSignalSpy spyHide(&dispatcher,
                       &ApplicationRequestDispatcher::hideWindowRequested);
    QSignalSpy spyToggle(&dispatcher,
                         &ApplicationRequestDispatcher::toggleWindowRequested);

    SingleInstanceClient client(serverName);

    IpcProtocol::Request req;
    req.requestId = "test-show";
    req.type = IpcProtocol::RequestType::ShowWindow;
    QVERIFY(client.sendRequest(req, 1000, 1000).isSuccess());
    QCOMPARE(spyShow.count(), 1);

    req.requestId = "test-hide";
    req.type = IpcProtocol::RequestType::HideWindow;
    QVERIFY(client.sendRequest(req, 1000, 1000).isSuccess());
    QCOMPARE(spyHide.count(), 1);

    req.requestId = "test-toggle";
    req.type = IpcProtocol::RequestType::ToggleWindow;
    QVERIFY(client.sendRequest(req, 1000, 1000).isSuccess());
    QCOMPARE(spyToggle.count(), 1);
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

  void testFramingMalformedResponseBadMagic() {
    QString serverName = "test-dummy-server-" +
                         QUuid::createUuid().toString(QUuid::WithoutBraces);
    QLocalServer dummyServer;
    dummyServer.listen(serverName);

    QObject::connect(
        &dummyServer, &QLocalServer::newConnection, &dummyServer, [&]() {
          QLocalSocket *client = dummyServer.nextPendingConnection();
          QObject::connect(
              client, &QLocalSocket::readyRead, client, [client]() {
                client->readAll();

                QByteArray payload;
                QDataStream payloadOut(&payload, QIODevice::WriteOnly);
                IpcProtocol::setupStream(payloadOut);
                quint32 badMagic = 0xBADF00D;
                payloadOut << badMagic
                           << static_cast<quint16>(IpcProtocol::VERSION)
                           << QString("frag-res")
                           << static_cast<quint16>(
                                  IpcProtocol::ResponseStatus::Accepted)
                           << QString("");

                QByteArray block;
                QDataStream out(&block, QIODevice::WriteOnly);
                IpcProtocol::setupStream(out);
                out << static_cast<quint32>(payload.size());
                block.append(payload);

                client->write(block);
                client->waitForBytesWritten(100);
                client->disconnectFromServer();
              });
        });

    SingleInstanceClient client(serverName);
    IpcProtocol::Request req;
    req.requestId = "frag-res";
    req.type = IpcProtocol::RequestType::ActivateWindow;

    ClientResult res = client.sendRequest(req, 1000, 5000);
    QCOMPARE(res.code, ClientResultCode::InvalidResponse);
  }

  void testFramingMalformedResponseTruncated() {
    QString serverName = "test-dummy-server-" +
                         QUuid::createUuid().toString(QUuid::WithoutBraces);
    QLocalServer dummyServer;
    dummyServer.listen(serverName);

    QObject::connect(
        &dummyServer, &QLocalServer::newConnection, &dummyServer, [&]() {
          QLocalSocket *client = dummyServer.nextPendingConnection();
          QObject::connect(
              client, &QLocalSocket::readyRead, client, [client]() {
                client->readAll();

                // Build response
                IpcProtocol::Response res;
                res.requestId = "frag-res";
                res.rawStatus =
                    static_cast<quint16>(IpcProtocol::ResponseStatus::Accepted);

                QByteArray payload;
                QDataStream payloadOut(&payload, QIODevice::WriteOnly);
                IpcProtocol::setupStream(payloadOut);
                payloadOut << res;

                // Truncate payload internally!
                payload.chop(5);

                QByteArray block;
                QDataStream out(&block, QIODevice::WriteOnly);
                IpcProtocol::setupStream(out);
                out << static_cast<quint32>(payload.size()); // valid length!
                block.append(payload);

                client->write(block);
                client->waitForBytesWritten(100);
                client->disconnectFromServer();
              });
        });

    SingleInstanceClient client(serverName);
    IpcProtocol::Request req;
    req.requestId = "frag-res";
    req.type = IpcProtocol::RequestType::ActivateWindow;

    ClientResult res = client.sendRequest(req, 1000, 5000);
    QCOMPARE(res.code, ClientResultCode::InvalidResponse);
  }

  void testFramingMalformedResponseZeroSize() {
    QString serverName = "test-dummy-server-" +
                         QUuid::createUuid().toString(QUuid::WithoutBraces);
    QLocalServer dummyServer;
    dummyServer.listen(serverName);

    QObject::connect(
        &dummyServer, &QLocalServer::newConnection, &dummyServer, [&]() {
          QLocalSocket *client = dummyServer.nextPendingConnection();
          QObject::connect(client, &QLocalSocket::readyRead, client,
                           [client]() {
                             client->readAll();

                             QByteArray block;
                             QDataStream out(&block, QIODevice::WriteOnly);
                             IpcProtocol::setupStream(out);
                             out << static_cast<quint32>(0); // ZERO size frame

                             client->write(block);
                             client->waitForBytesWritten(100);
                             client->disconnectFromServer();
                           });
        });

    SingleInstanceClient client(serverName);
    IpcProtocol::Request req;
    req.requestId = "frag-res";
    req.type = IpcProtocol::RequestType::ActivateWindow;

    ClientResult res = client.sendRequest(req, 1000, 5000);
    QCOMPARE(res.code, ClientResultCode::InvalidResponse);
  }

  void testFramingMalformedResponseOversized() {
    QString serverName = "test-dummy-server-" +
                         QUuid::createUuid().toString(QUuid::WithoutBraces);
    QLocalServer dummyServer;
    dummyServer.listen(serverName);

    QObject::connect(
        &dummyServer, &QLocalServer::newConnection, &dummyServer, [&]() {
          QLocalSocket *client = dummyServer.nextPendingConnection();
          QObject::connect(client, &QLocalSocket::readyRead, client,
                           [client]() {
                             client->readAll();

                             QByteArray block;
                             QDataStream out(&block, QIODevice::WriteOnly);
                             IpcProtocol::setupStream(out);
                             out << static_cast<quint32>(
                                 15 * 1024 * 1024); // OVERSIZED frame

                             client->write(block);
                             client->waitForBytesWritten(100);
                             client->disconnectFromServer();
                           });
        });

    SingleInstanceClient client(serverName);
    IpcProtocol::Request req;
    req.requestId = "frag-res";
    req.type = IpcProtocol::RequestType::ActivateWindow;

    ClientResult res = client.sendRequest(req, 1000, 5000);
    QCOMPARE(res.code, ClientResultCode::InvalidResponse);
  }

  void testFramingMalformedResponseUnknownStatus() {
    QString serverName = "test-dummy-server-" +
                         QUuid::createUuid().toString(QUuid::WithoutBraces);
    QLocalServer dummyServer;
    dummyServer.listen(serverName);

    QObject::connect(
        &dummyServer, &QLocalServer::newConnection, &dummyServer, [&]() {
          QLocalSocket *client = dummyServer.nextPendingConnection();
          QObject::connect(
              client, &QLocalSocket::readyRead, client, [client]() {
                client->readAll();

                QByteArray payload;
                QDataStream payloadOut(&payload, QIODevice::WriteOnly);
                IpcProtocol::setupStream(payloadOut);
                quint32 magic = IpcProtocol::MAGIC;
                quint16 unknownStatus = 999;
                payloadOut << magic
                           << static_cast<quint16>(IpcProtocol::VERSION)
                           << QString("frag-res") << unknownStatus
                           << QString("");

                QByteArray block;
                QDataStream out(&block, QIODevice::WriteOnly);
                IpcProtocol::setupStream(out);
                out << static_cast<quint32>(payload.size());
                block.append(payload);

                client->write(block);
                client->waitForBytesWritten(100);
                client->disconnectFromServer();
              });
        });

    SingleInstanceClient client(serverName);
    IpcProtocol::Request req;
    req.requestId = "frag-res";
    req.type = IpcProtocol::RequestType::ActivateWindow;

    ClientResult res = client.sendRequest(req, 1000, 5000);
    QCOMPARE(res.code, ClientResultCode::InvalidResponse);
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
                res.rawStatus =
                    static_cast<quint16>(IpcProtocol::ResponseStatus::Accepted);

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

  void testFramingIncompleteTransportFrame() {
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

  void testFramingTruncatedRequest() {
    QString serverName =
        "test-server-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ApplicationRequestDispatcher dispatcher;
    SingleInstanceServer server(serverName, &dispatcher);
    QVERIFY(server.tryAcquire());

    QSignalSpy spyActivate(
        &dispatcher, &ApplicationRequestDispatcher::activateWindowRequested);
    QSignalSpy spyProcess(
        &dispatcher, &ApplicationRequestDispatcher::processAddedLinesRequested);

    QLocalSocket socket;
    socket.connectToServer(serverName);
    QVERIFY(socket.waitForConnected(1000));

    IpcProtocol::Request req;
    req.requestId = "req-trunc";
    req.type = IpcProtocol::RequestType::AddInputs;
    req.payload = {"Truncate-Me-Please-123456789"};

    QByteArray payload;
    QDataStream payloadOut(&payload, QIODevice::WriteOnly);
    IpcProtocol::setupStream(payloadOut);
    payloadOut << req;

    // Remove the last 10 bytes so that magic, version, and requestId are fully
    // intact! But payload is truncated. So QDataStream will return Ok until it
    // hits the end unexpectedly.
    payload.chop(10);

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

        QCOMPARE(res.rawStatus,
                 static_cast<quint16>(
                     IpcProtocol::ResponseStatus::MalformedRequest));
        QCOMPARE(res.requestId,
                 QString("req-trunc")); // Since it read requestId safely before
                                        // aborting!
        loop.quit();
      }
    });
    loop.exec();

    QVERIFY(socket.state() == QLocalSocket::UnconnectedState ||
            expectedSize > 0);
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

    // Test 1: No primary exists, Action-bearing launch succeeds
    MockStartupSystem sysAction;
    StartupCoordinator coordAction(serverName, &sysAction, {2, 1, 1, 0});
    CoordinatorResult resAction =
        coordAction.coordinate(QStringList() << "/dummy/path" << "A.torrent");

    QCOMPARE(resAction.action, CoordinatorAction::RequestDelivered);
    QVERIFY(resAction.primaryLock ==
            nullptr); // short-lived launcher does not hold lock
    QCOMPARE(sysAction.spawnCount, 1);
    QCOMPARE(sysAction.spawnedProgram, QCoreApplication::applicationFilePath());
    QCOMPARE(sysAction.spawnedArgs.size(), 1);
    QCOMPARE(sysAction.spawnedArgs[0], QString("--hidden-primary"));
    QCOMPARE(sysAction.lastServerName, serverName);

    QCOMPARE(sysAction.sendRequestCount, 1);
    QCOMPARE(sysAction.lastSentRequest.type,
             IpcProtocol::RequestType::AddInputs);
    QCOMPARE(sysAction.lastSentRequest.payload.size(), 1);
    QCOMPARE(sysAction.lastSentRequest.payload[0], QString("A.torrent"));

    // Test 1b: No primary exists, Action-bearing launch with --show succeeds
    MockStartupSystem sysShow;
    StartupCoordinator coordShow(serverName, &sysShow, {2, 1, 1, 0});
    CoordinatorResult resShow =
        coordShow.coordinate(QStringList() << "/dummy/path" << "--show");

    QCOMPARE(resShow.action, CoordinatorAction::RequestDelivered);
    QCOMPARE(sysShow.spawnCount, 1);
    QCOMPARE(sysShow.spawnedArgs.size(), 1);
    QCOMPARE(sysShow.spawnedArgs[0], QString("--hidden-primary"));
    QCOMPARE(sysShow.sendRequestCount, 1);
    QCOMPARE(sysShow.lastSentRequest.type,
             IpcProtocol::RequestType::ShowWindow);

    // Test 1c: No primary exists, --hide should not spawn
    MockStartupSystem sysHide;
    StartupCoordinator coordHide(serverName, &sysHide, {2, 1, 1, 0});
    CoordinatorResult resHide =
        coordHide.coordinate(QStringList() << "/dummy/path" << "--hide");

    QCOMPARE(resHide.action, CoordinatorAction::RequestDelivered);
    QCOMPARE(sysHide.spawnCount, 0);
    QCOMPARE(sysHide.sendRequestCount, 0);

    // Test 1d: Conflicting flags fail
    MockStartupSystem sysConflict;
    StartupCoordinator coordConflict(serverName, &sysConflict, {2, 1, 1, 0});
    CoordinatorResult resConflict = coordConflict.coordinate(
        QStringList() << "/dummy/path" << "--show" << "--hide");
    QCOMPARE(resConflict.action, CoordinatorAction::RequestFailed);
    QCOMPARE(sysConflict.spawnCount, 0);

    // Test 2: Existing primary
    // Create a persistent primary lock here locally to simulate an existing
    // primary
    QString runtimePath =
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimePath.isEmpty())
      runtimePath = QDir::tempPath();
    QLockFile existingLock(QDir(runtimePath).filePath(serverName + ".lock"));
    existingLock.setStaleLockTime(0);
    QVERIFY(existingLock.tryLock(0));

    MockStartupSystem sysExisting;
    StartupCoordinator coordExisting(serverName, &sysExisting, {2, 1, 1, 0});
    CoordinatorResult resExisting =
        coordExisting.coordinate(QStringList() << "/dummy/path" << "B.torrent");

    QCOMPARE(resExisting.action, CoordinatorAction::RequestDelivered);
    QCOMPARE(sysExisting.spawnCount, 0); // MUST NOT SPAWN
    QCOMPARE(sysExisting.sendRequestCount, 1);
    QCOMPARE(sysExisting.lastSentRequest.payload[0], QString("B.torrent"));

    existingLock.unlock();
  }

  void testStartupCoordinatorFailures() {
    QString serverName =
        "test-failures-" + QUuid::createUuid().toString(QUuid::WithoutBraces);

    // C: Spawn failure
    MockStartupSystem sysFailSpawn;
    sysFailSpawn.spawnResult = false;
    StartupCoordinator coordSpawn(serverName, &sysFailSpawn, {2, 1, 1, 0});
    CoordinatorResult resSpawn =
        coordSpawn.coordinate(QStringList() << "/dummy/path" << "A.torrent");

    QCOMPARE(resSpawn.action, CoordinatorAction::SpawnFailed);
    QCOMPARE(sysFailSpawn.spawnCount, 1);
    QCOMPARE(sysFailSpawn.sendRequestCount,
             0); // Never sent request because spawn failed

    // D: Startup retry (Simulate transient connection issues recovering on
    // final retry) Since our mock returns a single static result, we can't
    // easily alternate failures to success without a custom mock state. But we
    // can test the retry loop itself if it exhausts. Or we can modify the mock
    // to alternate.

    TransientMockSystem sysTransient;
    StartupCoordinator coordTransient(serverName, &sysTransient, {5, 1, 1, 0});
    CoordinatorResult resTransient = coordTransient.coordinate(
        QStringList() << "/dummy/path" << "A.torrent");

    QCOMPARE(resTransient.action, CoordinatorAction::RequestDelivered);
    QCOMPARE(sysTransient.sendRequestCount,
             3); // Failed twice, succeeded third.

    // E: Deterministic rejection
    MockStartupSystem sysReject;
    sysReject.requestResultCode = ClientResultCode::RequestRejected;
    StartupCoordinator coordReject(serverName, &sysReject, {2, 1, 1, 0});
    CoordinatorResult resReject =
        coordReject.coordinate(QStringList() << "/dummy/path" << "A.torrent");

    QCOMPARE(resReject.action, CoordinatorAction::RequestFailed);
    QCOMPARE(sysReject.errorCount, 1);    // Visible error hook called
    QCOMPARE(sysReject.recoveryCount, 0); // NO recovery loop prompt

    // F: Recovery cancel after automatic attempts exhaust
    MockStartupSystem sysTimeout;
    sysTimeout.requestResultCode = ClientResultCode::ResponseTimeout;
    sysTimeout.recoveryResult = false; // User selects Cancel on recovery prompt
    StartupCoordinator coordTimeout(serverName, &sysTimeout,
                                    {2, 1, 1, 0}); // 2 retries
    CoordinatorResult resTimeout =
        coordTimeout.coordinate(QStringList() << "/dummy/path" << "A.torrent");

    QCOMPARE(resTimeout.action, CoordinatorAction::UserCancelled);
    QCOMPARE(sysTimeout.recoveryCount, 1);
    // Since it's a transport failure, showErrorMessage is NOT called.
    QCOMPARE(sysTimeout.errorCount, 0);
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

    QSignalSpy spyActivate(
        &dispatcher, &ApplicationRequestDispatcher::activateWindowRequested);
    QSignalSpy spyProcess(
        &dispatcher, &ApplicationRequestDispatcher::processAddedLinesRequested);

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
        // decodeRequest() reports BadMagic, server returns MalformedRequest.
        // requestId may be empty because the envelope was not trusted/decoded.
        QCOMPARE(res.rawStatus,
                 static_cast<quint16>(
                     IpcProtocol::ResponseStatus::MalformedRequest));
        QCOMPARE(res.requestId, QString(""));
        QCOMPARE(spyActivate.count(), 0);
        QCOMPARE(spyProcess.count(), 0);
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

        QCOMPARE(res.rawStatus,
                 static_cast<quint16>(
                     IpcProtocol::ResponseStatus::UnsupportedVersion));
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

        QCOMPARE(res.rawStatus,
                 static_cast<quint16>(
                     IpcProtocol::ResponseStatus::UnknownRequestType));
        QCOMPARE(res.requestId, QString("req-bad-type"));
        loop.quit();
      }
    });
    loop.exec();

    QVERIFY(socket.state() == QLocalSocket::UnconnectedState ||
            expectedSize > 0);
  }

  void testFramingUnsupportedVersionFutureStatus() {
    QString serverName = "test-dummy-server-" +
                         QUuid::createUuid().toString(QUuid::WithoutBraces);
    QLocalServer dummyServer;
    dummyServer.listen(serverName);

    QObject::connect(
        &dummyServer, &QLocalServer::newConnection, &dummyServer, [&]() {
          QLocalSocket *client = dummyServer.nextPendingConnection();
          QObject::connect(
              client, &QLocalSocket::readyRead, client, [client]() {
                client->readAll();

                QByteArray payload;
                QDataStream payloadOut(&payload, QIODevice::WriteOnly);
                IpcProtocol::setupStream(payloadOut);
                quint32 magic = IpcProtocol::MAGIC;
                quint16 version = IpcProtocol::VERSION + 1; // Future version!
                quint16 futureStatus = 999; // Unknown to current client
                payloadOut << magic << version << QString("req-unsupported")
                           << futureStatus << QString("");

                QByteArray block;
                QDataStream out(&block, QIODevice::WriteOnly);
                IpcProtocol::setupStream(out);
                out << static_cast<quint32>(payload.size());
                block.append(payload);

                client->write(block);
                client->waitForBytesWritten(100);
                client->disconnectFromServer();
              });
        });

    SingleInstanceClient client(serverName);
    IpcProtocol::Request req;
    req.requestId = "req-unsupported";
    req.type = IpcProtocol::RequestType::ActivateWindow;

    ClientResult res = client.sendRequest(req, 1000, 5000);
    // Even though status 999 is invalid to US, the version mismatch must take
    // precedence!
    QCOMPARE(res.code, ClientResultCode::UnsupportedProtocol);
  }

  void testDecoderTinyBuffers() {
    for (int size = 0; size < 4; ++size) {
      QByteArray shortBuffer;
      shortBuffer.resize(size);
      shortBuffer.fill(0);

      QDataStream inReq(&shortBuffer, QIODevice::ReadOnly);
      IpcProtocol::setupStream(inReq);
      IpcProtocol::Request req;
      QCOMPARE(IpcProtocol::decodeRequest(inReq, req),
               IpcProtocol::DecodeResult::Truncated);

      QDataStream inRes(&shortBuffer, QIODevice::ReadOnly);
      IpcProtocol::setupStream(inRes);
      IpcProtocol::Response res;
      QCOMPARE(IpcProtocol::decodeResponse(inRes, res),
               IpcProtocol::DecodeResult::Truncated);
    }
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
