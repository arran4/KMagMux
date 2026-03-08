#include "StorageManager.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QtConcurrent>
#include <utility>

StorageManager::StorageManager(QObject *parent)
    : QObject(parent), m_baseDir(QStandardPaths::writableLocation(
                           QStandardPaths::AppDataLocation)) {
  // Try to respect the lower-cased app name set in main() for AppDataLocation
  // But QStandardPaths usually uses appName() if it's set before.
  // We'll override if empty or fallback.

  QString appName = QCoreApplication::applicationName();
  if (appName.isEmpty()) {
    appName = "kmagmux";
  }

  // Set base directory
  if (m_baseDir.isEmpty()) {
    // Fallback for systems that might not return AppDataLocation correctly,
    // though unlikely with Qt
    m_baseDir = QDir::homePath() + "/.local/share/" + appName;
  }

  m_inboxDir = m_baseDir + "/inbox";
  m_queueDir = m_baseDir + "/queue";
  m_scheduledDir = m_baseDir + "/scheduled";
  m_holdDir = m_baseDir + "/hold";
  m_archiveDir = m_baseDir + "/archive";
  m_dataDir = m_baseDir + "/data";
  m_logsDir = m_baseDir + "/logs";
  m_managedDir = m_baseDir + "/managed"; // New folder for managed payloads

  m_watcher = new QFileSystemWatcher(this);
  connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
          &StorageManager::onDirectoryChanged);
}

bool StorageManager::init() {
  bool success = true;
  success &= createDirIfNotExists(m_baseDir);
  success &= createDirIfNotExists(m_inboxDir);
  success &= createDirIfNotExists(m_queueDir);
  success &= createDirIfNotExists(m_scheduledDir);
  success &= createDirIfNotExists(m_holdDir);
  success &= createDirIfNotExists(m_archiveDir);
  success &= createDirIfNotExists(m_dataDir);
  success &= createDirIfNotExists(m_logsDir);
  success &= createDirIfNotExists(m_managedDir);

  if (success) {
    qDebug() << "Storage initialized at:" << m_baseDir;

    // Initialize cache
    loadAllItems();

    // Start watching inbox
    if (m_watcher->addPath(m_inboxDir)) {
      qDebug() << "Watching inbox:" << m_inboxDir;
      // Initial scan to populate known files and process existing ones
      QStringList initialFiles = scanInbox();
      m_knownFiles = QSet<QString>(initialFiles.begin(), initialFiles.end());
      if (!m_knownFiles.isEmpty()) {
        (void)QtConcurrent::run([this, knownFiles = m_knownFiles]() {
          for (const QString &file : std::as_const(knownFiles)) {
            processNewFile(m_inboxDir + "/" + file);
          }
        });
      }
    } else {
      qWarning() << "Failed to watch inbox:" << m_inboxDir;
    }
  } else {
    qWarning() << "Failed to initialize storage at:" << m_baseDir;
  }
  return success;
}

bool StorageManager::createDirIfNotExists(const QString &path) {
  QDir dir(path);
  if (!dir.exists()) {
    if (!dir.mkpath(".")) {
      qWarning() << "Failed to create directory:" << path;
      return false;
    }
    qDebug() << "Created directory:" << path;
  }
  return true;
}

QString StorageManager::getBaseDir() const { return m_baseDir; }
QString StorageManager::getInboxDir() const { return m_inboxDir; }
QString StorageManager::getQueueDir() const { return m_queueDir; }
QString StorageManager::getDataDir() const { return m_dataDir; }
QString StorageManager::getManagedDir() const { return m_managedDir; }

QString StorageManager::getItemPath(const QString &id) const {
  // Basic sanitization
  QString safeId = id;
  safeId.replace("/", "_").replace("\\", "_");
  return m_dataDir + "/" + safeId + ".json";
}

bool StorageManager::saveItem(const Item &item) {
  if (item.id.isEmpty()) {
    qWarning() << "Cannot save item with empty ID";
    return false;
  }

  QString path = getItemPath(item.id);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning() << "Failed to open file for writing:" << path;
    return false;
  }

  QJsonDocument doc(item.toJson());
  if (file.write(doc.toJson()) == -1) {
    qWarning() << "Failed to write to file:" << path;
    return false;
  }

  // Update cache
  m_cache[item.id] = item;

  // Notify updates
  emit itemUpdated(item);

  return true;
}

void StorageManager::saveItems(const std::vector<Item> &items) {
  if (items.empty()) {
    return;
  }

  // Update cache immediately on the main thread to prevent stale data
  for (const Item &item : items) {
    m_cache[item.id] = item;
    emit itemUpdated(item);
  }

  (void)QtConcurrent::run([this, items]() {
    for (const Item &item : items) {
      if (item.id.isEmpty()) {
        continue;
      }

      QString path = getItemPath(item.id);
      QFile file(path);
      if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(item.toJson());
        file.write(doc.toJson());
      }
    }
  });
}

std::optional<Item> StorageManager::loadItem(const QString &id) {
  if (m_cacheInitialized && m_cache.contains(id)) {
    return m_cache[id];
  }

  QString path = getItemPath(id);
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "Failed to open file for reading:" << path;
    return std::nullopt;
  }

  QByteArray data = file.readAll();
  QJsonDocument doc = QJsonDocument::fromJson(data);
  if (doc.isNull() || !doc.isObject()) {
    qWarning() << "Failed to parse JSON from file:" << path;
    return std::nullopt;
  }

  Item item = Item::fromJson(doc.object());
  m_cache[item.id] = item;
  return item;
}

bool StorageManager::deleteItem(const QString &id) {
  std::optional<Item> optItem = loadItem(id);
  if (!optItem.has_value()) {
    return false;
  }

  Item item = optItem.value();

  // Remove the managed file if it exists
  if (item.metadata.contains("managedFile")) {
    QString managedPath = item.metadata["managedFile"].toString();
    if (!managedPath.isEmpty() && QFile::exists(managedPath)) {
      QFile::remove(managedPath);
    }
  } else if (item.sourcePath.startsWith(m_managedDir) &&
             QFile::exists(item.sourcePath)) {
    // Sometimes sourcePath points directly to the managed dir
    QFile::remove(item.sourcePath);
  }

  // Remove the JSON data file
  QString path = getItemPath(id);
  if (QFile::exists(path)) {
    if (!QFile::remove(path)) {
      qWarning() << "Failed to delete item data file:" << path;
      return false;
    }
  }

  m_cache.remove(id);

  emit itemDeleted(id);
  return true;
}

void StorageManager::deleteItems(const std::vector<QString> &ids) {
  if (ids.empty()) {
    return;
  }

  std::vector<QString> actuallyDeletedIds;
  actuallyDeletedIds.reserve(ids.size());

  for (const QString &id : ids) {
    std::optional<Item> optItem = loadItem(id);
    if (!optItem.has_value()) {
      continue;
    }

    Item item = optItem.value();

    // Remove the managed file if it exists
    if (item.metadata.contains("managedFile")) {
      QString managedPath = item.metadata["managedFile"].toString();
      if (!managedPath.isEmpty() && QFile::exists(managedPath)) {
        QFile::remove(managedPath);
      }
    } else if (item.sourcePath.startsWith(m_managedDir) &&
               QFile::exists(item.sourcePath)) {
      // Sometimes sourcePath points directly to the managed dir
      QFile::remove(item.sourcePath);
    }

    // Remove the JSON data file
    QString path = getItemPath(id);
    if (QFile::exists(path)) {
      if (!QFile::remove(path)) {
        qWarning() << "Failed to delete item data file:" << path;
        continue;
      }
    }

    m_cache.remove(id);
    actuallyDeletedIds.push_back(id);
  }

  if (!actuallyDeletedIds.empty()) {
    emit itemsDeleted(actuallyDeletedIds);
  }
}

std::vector<Item> StorageManager::loadAllItems() {
  std::vector<Item> items;

  if (m_cacheInitialized) {
    items.reserve(m_cache.size());
    for (auto it = m_cache.constBegin(); it != m_cache.constEnd(); ++it) {
      items.push_back(it.value());
    }
    return items;
  }

  m_cache.clear();
  QDirIterator it(m_dataDir, QStringList() << "*.json", QDir::Files);
  while (it.hasNext()) {
    QString path = it.next();
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
      QByteArray data = file.readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (!doc.isNull() && doc.isObject()) {
        Item item = Item::fromJson(doc.object());
        items.push_back(item);
        m_cache.insert(item.id, item);
      }
    }
  }
  m_cacheInitialized = true;
  return items;
}

std::vector<Item>
StorageManager::loadItemsByStates(const QList<ItemState> &states) {
  std::vector<Item> items;

  if (!m_cacheInitialized) {
    loadAllItems();
  }

  for (auto it = m_cache.constBegin(); it != m_cache.constEnd(); ++it) {
    if (states.contains(it.value().state)) {
      items.push_back(it.value());
    }
  }

  return items;
}

QStringList StorageManager::scanInbox() const {
  QDir dir(m_inboxDir);
  return dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
}

void StorageManager::onDirectoryChanged(const QString &path) {
  if (path == m_inboxDir) {
    QStringList currentFilesList = scanInbox();
    QSet<QString> currentFiles(currentFilesList.begin(),
                               currentFilesList.end());

    // Find new files
    QSet<QString> newFiles = currentFiles;
    newFiles.subtract(m_knownFiles);

    for (const QString &file : std::as_const(newFiles)) {
      processNewFile(m_inboxDir + "/" + file);
    }

    m_knownFiles = currentFiles;
  }
}

void StorageManager::processNewFile(const QString &filePath) {
  QFileInfo info(filePath);
  if (!info.exists() || !info.isFile())
    return;

  qDebug() << "Processing new file in inbox:" << filePath;

  Item newItem;
  newItem.id = QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" +
               info.fileName();
  newItem.state = ItemState::Unprocessed;
  newItem.sourcePath = filePath;
  newItem.createdTime = QDateTime::currentDateTime();

  QSettings settings;
  QVariant autoMoveSetting = settings.value("autoMoveInbox", 0);
  int actionIndex = 0;

  if (autoMoveSetting.typeId() == QMetaType::Bool) {
    actionIndex = autoMoveSetting.toBool() ? 2 : 0;
  } else {
    actionIndex = autoMoveSetting.toInt();
  }

  if (actionIndex == 1) { // Copy
    if (!moveToManaged(newItem, false, true)) {
      qWarning() << "Failed to automatically copy new item to managed storage:"
                 << newItem.sourcePath;
    } else {
      qDebug() << "Automatically copied new item to managed storage:"
               << newItem.sourcePath;
    }
  } else if (actionIndex == 2) { // Move (Delete Source)
    if (!moveToManaged(newItem, true, true)) {
      qWarning() << "Failed to automatically move new item to managed storage:"
                 << newItem.sourcePath;
    } else {
      qDebug() << "Automatically moved new item to managed storage:"
               << newItem.sourcePath;
    }
  }

  if (saveItem(newItem)) {
    emit itemAdded(newItem);
    qDebug() << "New item added from inbox watcher:" << newItem.id;
  }
}

bool StorageManager::moveToManaged(Item &item, bool deleteOriginal,
                                   bool skipSave) {
  if (item.sourcePath.startsWith("magnet:")) {
    // For magnets, we just create a .magnet file in managed dir
    QString filename = item.id + ".magnet";
    QString managedPath = m_managedDir + "/" + filename;
    QFile file(managedPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      file.write(item.sourcePath.toUtf8());
      file.close();
      // Update item to point to managed file? Or keep original source as
      // "magnet:"? The prompt says "store a .magnet text record file... when
      // queued/scheduled/saved" We can keep sourcePath as the magnet link, but
      // maybe store the managed file path in metadata
      QJsonObject meta = item.metadata;
      meta["managedFile"] = managedPath;
      item.metadata = meta;
      if (skipSave) {
        return true;
      }
      return saveItem(item);
    }
    return false;
  } else {
    // For .torrent files
    QFileInfo sourceInfo(item.sourcePath);
    if (!sourceInfo.exists())
      return false;

    QString filename = sourceInfo.fileName();
    // Maybe ensure uniqueness
    QString managedPath = m_managedDir + "/" + item.id + "_" + filename;

    bool success = false;
    if (deleteOriginal) {
      // Move
      // Qt's rename is a move
      if (QFile::rename(item.sourcePath, managedPath)) {
        success = true;
      } else {
        // If rename fails (cross-filesystem), try copy+remove
        if (QFile::copy(item.sourcePath, managedPath)) {
          if (QFile::remove(item.sourcePath)) {
            success = true;
          }
        }
      }
    } else {
      // Copy
      if (QFile::copy(item.sourcePath, managedPath)) {
        success = true;
      }
    }

    if (success) {
      // Update item to point to managed file?
      // "Replace original with nothing (or optionally keep a stub...)"
      // The item object should probably track where the actual payload is now.
      // Let's update sourcePath to the managed path if it was moved.
      if (deleteOriginal) {
        item.sourcePath = managedPath;
      } else {
        QJsonObject meta = item.metadata;
        meta["managedFile"] = managedPath;
        item.metadata = meta;
      }
      if (skipSave) {
        return true;
      }
      return saveItem(item);
    }
    return false;
  }
}
