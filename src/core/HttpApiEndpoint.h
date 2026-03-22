#ifndef HTTPAPIENDPOINT_H
#define HTTPAPIENDPOINT_H

#include <QByteArray>
#include <QMap>
#include <QString>

struct HttpApiEndpoint {
  QString name;
  QString description;
  QString method; // GET, POST, PUT, DELETE, etc.
  QString url;
  QMap<QString, QString> headers;
  QByteArray body;

  bool isMultipart = false;
  // Key = field name, Value = string value or "file:///path/to/file"
  QMap<QString, QString> multipartParts;
};

#endif // HTTPAPIENDPOINT_H
