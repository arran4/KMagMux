#ifndef ITEM_H
#define ITEM_H

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QMutex>
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
  Item() : state(ItemState::Unprocessed) {}
  Item(const Item &other)
      : id(other.id), state(other.state), sourcePath(other.sourcePath),
        destinationPath(other.destinationPath), connectorId(other.connectorId),
        createdTime(other.createdTime), scheduledTime(other.scheduledTime),
        metadata(other.metadata) {
    QMutexLocker locker(&other.m_cacheMutex);
    m_cachedDisplayName = other.m_cachedDisplayName;
    m_cachedSourcePath = other.m_cachedSourcePath;
  }

  Item &operator=(const Item &other) {
    if (this != &other) {
      id = other.id;
      state = other.state;
      sourcePath = other.sourcePath;
      destinationPath = other.destinationPath;
      connectorId = other.connectorId;
      createdTime = other.createdTime;
      scheduledTime = other.scheduledTime;
      metadata = other.metadata;

      QMutexLocker locker1(&m_cacheMutex);
      QMutexLocker locker2(&other.m_cacheMutex);
      m_cachedDisplayName = other.m_cachedDisplayName;
      m_cachedSourcePath = other.m_cachedSourcePath;
    }
    return *this;
  }

  QString id;
  ItemState state;
  QString sourcePath; // Original path or magnet link
  QString destinationPath;
  QString connectorId;
  QDateTime createdTime;
  QDateTime scheduledTime;
  QJsonObject metadata; // Flexible metadata storage

  void addHistory(const QString &message);

  mutable QString m_cachedDisplayName;
  mutable QString m_cachedSourcePath;
  mutable QMutex m_cacheMutex;

  QString getDisplayName() const;

  // Serialization helpers
  QJsonObject toJson() const;
  static Item fromJson(const QJsonObject &json);

  // Helper to get string representation of state
  QString stateToString() const;
  static ItemState stringToState(const QString &s);
};

Q_DECLARE_METATYPE(Item)

#endif // ITEM_H
