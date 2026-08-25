#ifndef SINGLEINSTANCECLIENT_H
#define SINGLEINSTANCECLIENT_H

#include "IpcProtocol.h"
#include <QString>

enum class ClientResultCode {
  Accepted,
  NoPrimary,
  ConnectFailed,
  WriteFailed,
  ResponseTimeout,
  ConnectionClosed,
  InvalidResponse,
  RequestRejected,
  UnsupportedProtocol
};

struct ClientResult {
  ClientResultCode code;
  QString diagnostic;

  bool isSuccess() const { return code == ClientResultCode::Accepted; }
};

class SingleInstanceClient {
public:
  explicit SingleInstanceClient(const QString &serverName);

  ClientResult sendRequest(const IpcProtocol::Request &request,
                           int connectTimeoutMs = 500,
                           int responseTimeoutMs = 5000);

private:
  QString m_serverName;
};

#endif // SINGLEINSTANCECLIENT_H
