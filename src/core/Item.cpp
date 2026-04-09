#include "Item.h"
#include <QFileInfo>
#include <QJsonDocument>
#include <QUrl>
#include <QUrlQuery>

QString Item::getDisplayName() const {
  const QMutexLocker<QMutex> locker(&m_cacheMutex);
  if (sourcePath == m_cachedSourcePath && !m_cachedDisplayName.isEmpty()) {
    return m_cachedDisplayName;
  }

  QString result;
  if (sourcePath.startsWith("magnet:?")) {
    const QUrl url(sourcePath);
    const QUrlQuery query(url);
    if (query.hasQueryItem("dn")) {
      result = QUrl::fromPercentEncoding(query.queryItemValue("dn").toUtf8());
    } else if (query.hasQueryItem("tr")) {
      result = "Magnet Link (No Name)";
    } else {
      // maybe xt infohash?
      const QString fullQuery = url.query();
      const int xtIdx = static_cast<int>(fullQuery.indexOf("xt="));
      if (xtIdx != -1) {
        int endIdx = static_cast<int>(fullQuery.indexOf('&', xtIdx));
        if (endIdx == -1) {
          endIdx = static_cast<int>(fullQuery.length());
        }
        result = fullQuery.mid(xtIdx + 3, endIdx - xtIdx - 3);
      } else {
        result = "Magnet Link";
      }
    }
  } else {
    const QFileInfo fileInfo(sourcePath);
    const QString name = fileInfo.fileName();
    if (!name.isEmpty()) {
      result = QUrl::fromPercentEncoding(name.toUtf8());
    } else {
      result = sourcePath;
    }
  }

  m_cachedSourcePath = sourcePath;
  m_cachedDisplayName = result;
  return result;
}

QJsonObject Item::toJson() const {
  QJsonObject json;
  json["id"] = id;
  json["state"] = stateToString();
  json["sourcePath"] = sourcePath;
  json["destinationPath"] = destinationPath;
  json["connectorId"] = connectorId;
  json["createdTime"] = createdTime.toString(Qt::ISODate);
  if (scheduledTime.isValid()) {
    json["scheduledTime"] = scheduledTime.toString(Qt::ISODate);
  } else {
    json["scheduledTime"] = QJsonValue::Null;
  }
  json["metadata"] = metadata;
  return json;
}

Item Item::fromJson(const QJsonObject &json) {
  Item item;
  item.id = json["id"].toString();
  item.state = stringToState(json["state"].toString());
  item.sourcePath = json["sourcePath"].toString();
  item.destinationPath = json["destinationPath"].toString();
  item.connectorId = json["connectorId"].toString();
  item.createdTime =
      QDateTime::fromString(json["createdTime"].toString(), Qt::ISODate);
  if (json.contains("scheduledTime") && !json["scheduledTime"].isNull()) {
    item.scheduledTime =
        QDateTime::fromString(json["scheduledTime"].toString(), Qt::ISODate);
  }
  item.metadata = json["metadata"].toObject();
  return item;
}

QString Item::stateToString() const {
  switch (state) {
  case ItemState::Unprocessed:
    return "Unprocessed";
  case ItemState::Queued:
    return "Queued";
  case ItemState::Scheduled:
    return "Scheduled";
  case ItemState::Held:
    return "Held";
  case ItemState::Done:
    return "Done";
  case ItemState::Failed:
    return "Failed";
  case ItemState::Archived:
    return "Archived";
  default:
    return "Unknown";
  }
}

ItemState Item::stringToState(const QString &str) {
  if (str == "Unprocessed") {
    return ItemState::Unprocessed;
  }
  if (str == "Queued") {
    return ItemState::Queued;
  }
  if (str == "Scheduled") {
    return ItemState::Scheduled;
  }
  if (str == "Held") {
    return ItemState::Held;
  }
  if (str == "Done" || str == "Dispatched")
    return ItemState::Done;
  if (str == "Failed") {
    return ItemState::Failed;
  }
  if (str == "Archived") {
    return ItemState::Archived;
  }
  return ItemState::Unprocessed;
}

#include <QJsonArray>

void Item::addHistory(const QString &message) {
  QJsonArray history = metadata["history"].toArray();
  QJsonObject entry;
  entry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
  entry["message"] = message;
  history.append(entry);
  metadata["history"] = history;
}
