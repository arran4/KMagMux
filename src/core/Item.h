#ifndef ITEM_H
#define ITEM_H

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QString>

enum class ItemState {
  Unprocessed,
  Queued,
  Scheduled,
  Held,
  Done,
  Failed,
  Archived
};

struct Item {
  QString id;
  ItemState state;
  QString sourcePath; // Original path or magnet link
  QString destinationPath;
  QString connectorId;
  QDateTime createdTime;
  QDateTime scheduledTime;
  QJsonObject metadata; // Flexible metadata storage

  // Serialization helpers
  QJsonObject toJson() const;
  static Item fromJson(const QJsonObject &json);

  // Helper to get string representation of state
  QString stateToString() const;
  static ItemState stringToState(const QString &s);
};

Q_DECLARE_METATYPE(Item)

#endif // ITEM_H
