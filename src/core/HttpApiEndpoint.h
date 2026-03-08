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
};

#endif // HTTPAPIENDPOINT_H
