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

  QByteArray payload;
  QDataStream payloadOut(&payload, QIODevice::WriteOnly);
  IpcProtocol::setupStream(payloadOut);
  payloadOut << request;

  QByteArray block;
  QDataStream out(&block, QIODevice::WriteOnly);
  IpcProtocol::setupStream(out);
  out << static_cast<quint32>(payload.size());
  block.append(payload);

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

  quint32 expectedSize = 0;
  while (timer.isActive() && socket.state() == QLocalSocket::ConnectedState) {
    if (expectedSize == 0 &&
        socket.bytesAvailable() >= static_cast<qint64>(sizeof(quint32))) {
      QDataStream in(&socket);
      IpcProtocol::setupStream(in);
      in >> expectedSize;

      if (expectedSize == 0 || expectedSize > IpcProtocol::MAX_FRAME_SIZE) {
        qWarning() << "Invalid frame size received from server:"
                   << expectedSize;
        break;
      }
    }

    if (expectedSize > 0 &&
        socket.bytesAvailable() >= static_cast<qint64>(expectedSize)) {
      QByteArray frame = socket.read(expectedSize);
      QDataStream in(&frame, QIODevice::ReadOnly);
      IpcProtocol::setupStream(in);
      in >> response;

      if (in.status() == QDataStream::Ok && in.atEnd()) {
        responseReceived = true;
      } else {
        // Frame was malformed
        response.status = IpcProtocol::ResponseStatus::MalformedRequest;
        response.version = 0;
        // Keep loop running to fail with timeout/disconnect, or break.
        // Since frame is malformed, we can't reliably read more.
        break;
      }
      break;
    }

    if (!responseReceived) {
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
