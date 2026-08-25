#include "SingleInstanceServer.h"
#include "ApplicationRequestDispatcher.h"
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QLocalSocket>
#include <QStandardPaths>

SingleInstanceServer::SingleInstanceServer(
    const QString &serverName, ApplicationRequestDispatcher *dispatcher,
    QObject *parent)
    : QObject(parent), m_serverName(serverName), m_dispatcher(dispatcher),
      m_lockFile(nullptr) {}

SingleInstanceServer::~SingleInstanceServer() {
  m_server.close();
  if (m_lockFile) {
    m_lockFile->unlock();
    delete m_lockFile;
  }
}

bool SingleInstanceServer::tryAcquire() {
  QString runtimePath =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  if (runtimePath.isEmpty()) {
    runtimePath = QDir::tempPath();
  }
  QString lockFilePath = QDir(runtimePath).filePath(m_serverName + ".lock");

  m_lockFile = new QLockFile(lockFilePath);
  m_lockFile->setStaleLockTime(0); // Only considered stale if process is dead

  if (!m_lockFile->tryLock(100)) { // wait up to 100ms
    qDebug() << "Could not acquire primary lock. Another instance is likely "
                "running.";
    delete m_lockFile;
    m_lockFile = nullptr;
    return false;
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

void SingleInstanceServer::handleNewConnection() {
  QLocalSocket *client = m_server.nextPendingConnection();
  connect(client, &QLocalSocket::readyRead, this, [this, client]() {
    QDataStream dataStream(client);
    dataStream.startTransaction();

    IpcProtocol::Request request;
    dataStream >> request;

    if (!dataStream.commitTransaction()) {
      return; // Wait for more data
    }

    IpcProtocol::Response response;
    response.requestId = request.requestId;

    if (!request.isValid()) {
      response.status = IpcProtocol::ResponseStatus::MalformedRequest;
      response.errorMessage = "Malformed request";
    } else if (m_dispatcher) {
      response.status = m_dispatcher->dispatch(request);
      if (response.status != IpcProtocol::ResponseStatus::Accepted) {
        response.errorMessage = "Failed to dispatch request";
      }
    } else {
      response.status = IpcProtocol::ResponseStatus::InternalError;
      response.errorMessage = "No dispatcher available";
    }

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out << response;
    client->write(block);
    client->waitForBytesWritten(1000);

    // Since we got a full request and responded, we can disconnect.
    // The client will read the response and close its end.
    client->disconnectFromServer();
  });

  connect(client, &QLocalSocket::disconnected, client,
          &QLocalSocket::deleteLater);
}
