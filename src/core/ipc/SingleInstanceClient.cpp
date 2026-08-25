#include "SingleInstanceClient.h"
#include <QDataStream>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLocalSocket>
#include <QTimer>

SingleInstanceClient::SingleInstanceClient(const QString &serverName)
    : m_serverName(serverName) {}

ClientResult
SingleInstanceClient::sendRequest(const IpcProtocol::Request &request,
                                  int connectTimeoutMs, int responseTimeoutMs) {
  QLocalSocket socket;

  socket.connectToServer(m_serverName);
  if (!socket.waitForConnected(connectTimeoutMs)) {
    return {ClientResultCode::ConnectFailed,
            "Could not connect to primary instance."};
  }

  QByteArray block;
  QDataStream out(&block, QIODevice::WriteOnly);
  out << request;

  if (socket.write(block) == -1 ||
      !socket.waitForBytesWritten(connectTimeoutMs)) {
    return {ClientResultCode::WriteFailed,
            "Failed to write request to socket."};
  }

  QEventLoop loop;
  QTimer timer;
  timer.setSingleShot(true);

  QObject::connect(&socket, &QLocalSocket::readyRead, &loop, &QEventLoop::quit);
  QObject::connect(&socket, &QLocalSocket::disconnected, &loop,
                   &QEventLoop::quit);
  QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

  timer.start(responseTimeoutMs);

  IpcProtocol::Response response;
  bool responseReceived = false;

  while (timer.isActive() && socket.state() == QLocalSocket::ConnectedState) {
    if (socket.bytesAvailable() > 0) {
      QDataStream in(&socket);
      in.startTransaction();
      in >> response;
      if (in.commitTransaction()) {
        responseReceived = true;
        break;
      }
    } else {
      loop.exec();
    }
  }
  timer.stop();

  if (!responseReceived) {
    if (socket.state() != QLocalSocket::ConnectedState) {
      return {ClientResultCode::ConnectionClosed,
              "Connection closed by primary before response."};
    }
    return {ClientResultCode::ResponseTimeout,
            "Timed out waiting for response from primary."};
  }

  if (response.version != IpcProtocol::VERSION) {
    return {ClientResultCode::UnsupportedProtocol,
            "Unsupported protocol version from primary."};
  }

  if (response.requestId != request.requestId) {
    return {ClientResultCode::InvalidResponse,
            "Mismatched request ID in response."};
  }

  if (response.status == IpcProtocol::ResponseStatus::Accepted) {
    return {ClientResultCode::Accepted, "Request accepted."};
  }

  return {ClientResultCode::RequestRejected,
          QString("Request rejected: %1").arg(response.errorMessage)};
}
