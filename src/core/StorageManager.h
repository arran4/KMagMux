#ifndef STORAGEMANAGER_H
#define STORAGEMANAGER_H

#include "Item.h"
#include <QDir>
#include <QFileSystemWatcher>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <optional>
#include <vector>

class StorageManager : public QObject {
  Q_OBJECT

public:
  explicit StorageManager(QObject *parent = nullptr);
  bool init();

  // Directory paths
  QString getBaseDir() const;
  QString getInboxDir() const;
  QString getQueueDir() const;
  QString getDataDir() const;
  QString
  getManagedDir() const; // New: Directory for managed torrent/magnet files

  // Persistence
  bool saveItem(const Item &item);
  void saveItems(const std::vector<Item> &items);
  std::optional<Item> loadItem(const QString &id);
  std::vector<Item> loadAllItems();
  std::vector<Item> loadItemsByStates(const QList<ItemState> &states) const;
  bool deleteItem(const QString &id);

  // Scanning
  QStringList scanInbox() const;

  // File Management
  bool moveToManaged(Item &item, bool deleteOriginal, bool skipSave = false);

signals:
  void itemAdded(const Item &item);
  void itemUpdated(const Item &item);
  void itemDeleted(const QString &id);

private slots:
  void onDirectoryChanged(const QString &path);

private:
  QString m_baseDir;
  QString m_inboxDir;
  QString m_queueDir;
  QString m_scheduledDir;
  QString m_holdDir;
  QString m_archiveDir;
  QString m_dataDir;
  QString m_logsDir;
  QString m_managedDir; // New

  QFileSystemWatcher *m_watcher;
  QSet<QString> m_knownFiles; // To track new files vs existing

  QMap<QString, Item> m_itemCache;

  bool createDirIfNotExists(const QString &path);
  QString getItemPath(const QString &id) const;
  void processNewFile(const QString &filePath);
};

#endif // STORAGEMANAGER_H
