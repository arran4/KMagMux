#include "SingleInstanceServer.h"
#include "ApplicationRequestDispatcher.h"
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QTimer>

SingleInstanceServer::SingleInstanceServer(
    const QString &serverName, ApplicationRequestDispatcher *dispatcher,
    QObject *parent)
    : QObject(parent), m_serverName(serverName), m_dispatcher(dispatcher),
      m_lockFile(nullptr) {}

SingleInstanceServer::~SingleInstanceServer() {
  m_server.close();
  if (m_lockFile) {
    m_lockFile->unlock();
  }
}

bool SingleInstanceServer::tryAcquire(std::unique_ptr<QLockFile> existingLock) {
  if (existingLock) {
    m_lockFile = std::move(existingLock);
  } else {
    QString runtimePath =
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimePath.isEmpty()) {
      runtimePath = QDir::tempPath();
    }
    QString lockFilePath = QDir(runtimePath).filePath(m_serverName + ".lock");

    m_lockFile = std::make_unique<QLockFile>(lockFilePath);
    m_lockFile->setStaleLockTime(0); // Only considered stale if process is dead

    if (!m_lockFile->tryLock(0)) {
      qDebug() << "Could not acquire primary lock. Another instance is likely "
                  "running.";
      m_lockFile.reset();
      return false;
    }
  }

  // We acquired the lock. Now start the local server.
  QLocalServer::removeServer(m_serverName);
  m_server.setSocketOptions(QLocalServer::UserAccessOption);
  if (!m_server.listen(m_serverName)) {
    qWarning() << "Failed to start local server:" << m_server.errorString();
    // Still acquired lock, but server failed.
    return false;
  }

  connect(&m_server, &QLocalServer::newConnection, this,
          &SingleInstanceServer::handleNewConnection);
  qDebug() << "Started SingleInstanceServer on" << m_serverName;
  return true;
}

void SingleInstanceServer::disconnectClient(QLocalSocket *client) {
  if (client) {
    client->disconnectFromServer();
    client->deleteLater();
  }
}

void SingleInstanceServer::handleNewConnection() {
  QLocalSocket *client = m_server.nextPendingConnection();

  // Track expected size per client connection using a dynamic property
  client->setProperty("expectedSize", 0);

  // Set a strict 5 second timeout to prevent hung clients
  QTimer *timeoutTimer = new QTimer(client);
  timeoutTimer->setSingleShot(true);
  timeoutTimer->start(5000);

  connect(timeoutTimer, &QTimer::timeout, this, [this, client]() {
    qWarning() << "Client connection timed out while reading.";
    disconnectClient(client);
  });

  connect(
      client, &QLocalSocket::readyRead, this, [this, client, timeoutTimer]() {
        quint32 expectedSize = client->property("expectedSize").toUInt();

        if (expectedSize == 0 &&
            client->bytesAvailable() >= static_cast<qint64>(sizeof(quint32))) {
          QDataStream in(client);
          IpcProtocol::setupStream(in);
          in >> expectedSize;

          // Basic sanity check on frame size to prevent massive allocations
          // (e.g. 10MB limit)
          if (expectedSize == 0 || expectedSize > IpcProtocol::MAX_FRAME_SIZE) {
            qWarning() << "Invalid frame size received:" << expectedSize;
            disconnectClient(client);
            return;
          }

          client->setProperty("expectedSize", expectedSize);
        }

        if (expectedSize > 0 &&
            client->bytesAvailable() >= static_cast<qint64>(expectedSize)) {
          timeoutTimer->stop(); // Read finished

          QByteArray frame = client->read(expectedSize);
          QDataStream in(&frame, QIODevice::ReadOnly);
          IpcProtocol::setupStream(in);
          IpcProtocol::Request request;
          const auto decodeResult = IpcProtocol::decodeRequest(in, request);

          IpcProtocol::Response response;
          response.requestId = request.requestId;

          switch (decodeResult) {
          case IpcProtocol::DecodeResult::BadMagic:
          case IpcProtocol::DecodeResult::Truncated:
            response.rawStatus = static_cast<quint16>(
                IpcProtocol::ResponseStatus::MalformedRequest);
            response.errorMessage = "Malformed request";
            break;
          case IpcProtocol::DecodeResult::Success:
            if (request.version != IpcProtocol::VERSION) {
              response.rawStatus = static_cast<quint16>(
                  IpcProtocol::ResponseStatus::UnsupportedVersion);
              response.errorMessage = "Unsupported protocol version";
            } else if (request.requestId.isEmpty()) {
              response.rawStatus = static_cast<quint16>(
                  IpcProtocol::ResponseStatus::MalformedRequest);
              response.errorMessage = "Malformed request";
            } else if (request.type !=
                           IpcProtocol::RequestType::ActivateWindow &&
                       request.type != IpcProtocol::RequestType::AddInputs) {
              response.rawStatus = static_cast<quint16>(
                  IpcProtocol::ResponseStatus::UnknownRequestType);
              response.errorMessage = "Unknown request type";
            } else if (m_dispatcher) {
              IpcProtocol::ResponseStatus status =
                  m_dispatcher->dispatch(request);
              response.rawStatus = static_cast<quint16>(status);
              if (status != IpcProtocol::ResponseStatus::Accepted) {
                response.errorMessage = "Failed to dispatch request";
              }
            } else {
              response.rawStatus = static_cast<quint16>(
                  IpcProtocol::ResponseStatus::InternalError);
              response.errorMessage = "No dispatcher available";
            }
            break;
          }

          QByteArray payload;
          QDataStream payloadOut(&payload, QIODevice::WriteOnly);
          IpcProtocol::setupStream(payloadOut);
          payloadOut << response;

          QByteArray block;
          QDataStream out(&block, QIODevice::WriteOnly);
          IpcProtocol::setupStream(out);
          out << static_cast<quint32>(payload.size());
          block.append(payload);

          client->write(block);
          client->waitForBytesWritten(1000);

          // We handled one request on this connection.
          disconnectClient(client);
        }
      });

  connect(client, &QLocalSocket::disconnected, client,
          &QLocalSocket::deleteLater);
}
