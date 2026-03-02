#include "Item.h"
#include <QJsonDocument>

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
  case ItemState::Dispatched:
    return "Dispatched";
  case ItemState::Failed:
    return "Failed";
  case ItemState::Archived:
    return "Archived";
  default:
    return "Unknown";
  }
}

ItemState Item::stringToState(const QString &s) {
  if (s == "Unprocessed")
    return ItemState::Unprocessed;
  if (s == "Queued")
    return ItemState::Queued;
  if (s == "Scheduled")
    return ItemState::Scheduled;
  if (s == "Held")
    return ItemState::Held;
  if (s == "Dispatched")
    return ItemState::Dispatched;
  if (s == "Failed")
    return ItemState::Failed;
  if (s == "Archived")
    return ItemState::Archived;
  return ItemState::Unprocessed;
}
