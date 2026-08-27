#ifndef IPCPROTOCOL_H
#define IPCPROTOCOL_H

#include <QDataStream>
#include <QString>
#include <QStringList>

namespace IpcProtocol {

const quint32 MAGIC = 0x4B4D474D;                // KMGm
const quint32 MAX_FRAME_SIZE = 10 * 1024 * 1024; // 10 MB

const quint16 VERSION = 1;

enum class RequestType : quint16 { ActivateWindow = 1, AddInputs = 2 };

enum class ResponseStatus : quint16 {
  Accepted = 0,
  UnsupportedVersion = 1,
  UnknownRequestType = 2,
  MalformedRequest = 3,
  DispatchFailed = 4,
  BusyOrShuttingDown = 5,
  InternalError = 6
};

struct Request {
  quint16 version = VERSION;
  QString requestId;
  RequestType type = RequestType::ActivateWindow;
  QStringList payload;

  bool isValid() const { return version == VERSION && !requestId.isEmpty(); }
};

struct Response {
  quint16 version = VERSION;
  QString requestId;
  ResponseStatus status = ResponseStatus::MalformedRequest;
  QString errorMessage;
};

// Serialization
inline void setupStream(QDataStream &stream) {
  stream.setVersion(QDataStream::Qt_6_5);
  stream.setByteOrder(QDataStream::BigEndian);
}

inline QDataStream &operator<<(QDataStream &out, const Request &req) {
  out << MAGIC << req.version << req.requestId << static_cast<quint16>(req.type)
      << req.payload;
  return out;
}

inline QDataStream &operator<<(QDataStream &out, const Response &res) {
  out << MAGIC << res.version << res.requestId
      << static_cast<quint16>(res.status) << res.errorMessage;
  return out;
}

enum class DecodeResult { Success, BadMagic, Truncated };

inline DecodeResult decodeRequest(QDataStream &in, Request &req) {
  quint32 magic;
  in >> magic;
  if (magic != MAGIC) {
    return DecodeResult::BadMagic;
  }
  quint16 typeInt;
  in >> req.version >> req.requestId >> typeInt >> req.payload;
  req.type = static_cast<RequestType>(typeInt);

  if (in.status() != QDataStream::Ok || !in.atEnd()) {
    return DecodeResult::Truncated;
  }
  return DecodeResult::Success;
}

inline DecodeResult decodeResponse(QDataStream &in, Response &res) {
  quint32 magic;
  in >> magic;
  if (magic != MAGIC) {
    return DecodeResult::BadMagic;
  }
  quint16 statusInt;
  in >> res.version >> res.requestId >> statusInt >> res.errorMessage;
  res.status = static_cast<ResponseStatus>(statusInt);

  if (in.status() != QDataStream::Ok || !in.atEnd()) {
    return DecodeResult::Truncated;
  }
  return DecodeResult::Success;
}

inline QDataStream &operator>>(QDataStream &in, Request &req) {
  decodeRequest(in, req);
  return in;
}

inline QDataStream &operator>>(QDataStream &in, Response &res) {
  decodeResponse(in, res);
  return in;
}

} // namespace IpcProtocol

#endif // IPCPROTOCOL_H
