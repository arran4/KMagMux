#include "Engine.h"
#include "Constants.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonObject>
#include <QLibrary>
#include <QPluginLoader>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QVersionNumber>

Engine::Engine(StorageManager *storage, QObject *parent)
    : QObject(parent), m_storage(storage), m_paused(false) {
  m_timer = new QTimer(this);
  // Check queue every 5 seconds (configurable later)
  m_timer->setInterval(5000);
  connect(m_timer, &QTimer::timeout, this, &Engine::processQueue);

  // Load plugins
  QString appDir = QCoreApplication::applicationDirPath();
  QStringList pluginPaths;

  // Dev path
  pluginPaths << QDir::cleanPath(appDir + "/plugins");

  // CMake binary output path (when developing and running from build/ without
  // install)
  pluginPaths << QDir::cleanPath(appDir + "/../plugins");

  // CMake binary output path (when developing and running using target
  // "kmagmux" directly via CLion/cmake)
  pluginPaths << QDir::cleanPath(appDir + "/plugins/qbittorrent");
  pluginPaths << QDir::cleanPath(appDir + "/src/plugins/qbittorrent");
  pluginPaths << QDir::cleanPath(appDir + "/../src/plugins/qbittorrent");
  pluginPaths << QDir::cleanPath(appDir + "/plugins/torbox");
  pluginPaths << QDir::cleanPath(appDir + "/src/plugins/torbox");
  pluginPaths << QDir::cleanPath(appDir + "/../src/plugins/torbox");
  pluginPaths << QDir::cleanPath(appDir + "/plugins/putio");
  pluginPaths << QDir::cleanPath(appDir + "/src/plugins/putio");
  pluginPaths << QDir::cleanPath(appDir + "/../src/plugins/putio");
  pluginPaths << QDir::cleanPath(appDir + "/plugins/premiumize");
  pluginPaths << QDir::cleanPath(appDir + "/src/plugins/premiumize");
  pluginPaths << QDir::cleanPath(appDir + "/../src/plugins/premiumize");

  // Explicit IDE binary output path
  pluginPaths << QDir::cleanPath(appDir + "/../../cmake-build-debug/plugins");
  pluginPaths << QDir::cleanPath(appDir + "/../cmake-build-debug/plugins");
  pluginPaths << QDir::cleanPath(appDir + "/cmake-build-debug/plugins");

  // Install path (relative to executable)
#ifdef KMAGMUX_REL_PLUGIN_DIR
  pluginPaths << QDir::cleanPath(appDir + "/" +
                                 QStringLiteral(KMAGMUX_REL_PLUGIN_DIR));
#endif

  struct PluginInfo {
    QString filePath;
    QVersionNumber version;
    bool isDevelopment;
  };

  QMap<QString, PluginInfo> bestPlugins;

  for (const QString &path : pluginPaths) {
    QDir pluginsDir(path);
    if (!pluginsDir.exists()) {
      continue;
    }

    qDebug() << "Looking for plugins in:" << pluginsDir.absolutePath();

    for (QString fileName : pluginsDir.entryList(QDir::Files)) {
      QString filePath = pluginsDir.absoluteFilePath(fileName);
      QPluginLoader pluginLoader(filePath);

      // Check metadata before instantiating to avoid loading non-plugin
      // libraries blockingly
      QJsonObject meta = pluginLoader.metaData();
      if (meta.value("IID").toString() != "com.kmagmux.Connector/1.0") {
        continue;
      }

      QJsonObject metaDataObj = meta.value("MetaData").toObject();
      QString versionStr = metaDataObj.value("version").toString();
      if (versionStr.isEmpty()) {
        versionStr = meta.value("version").toString();
      }

      QString name = metaDataObj.value("name").toString();
      if (name.isEmpty()) {
        name = fileName;
      }

      bool isDev = versionStr.contains("development", Qt::CaseInsensitive) ||
                   versionStr.contains("dev", Qt::CaseInsensitive);

      // Clean version string for QVersionNumber parsing
      QString cleanVersionStr = versionStr;
      cleanVersionStr.remove(QRegularExpression("[^0-9\\.]"));
      QVersionNumber version = QVersionNumber::fromString(cleanVersionStr);

      auto it = bestPlugins.find(name);
      if (it == bestPlugins.end()) {
        bestPlugins.insert(name, {filePath, version, isDev});
      } else {
        PluginInfo existing = it.value();
        bool shouldReplace = false;

        if (version > existing.version) {
          shouldReplace = true;
        } else if (version == existing.version) {
          if (isDev && !existing.isDevelopment) {
            shouldReplace = true;
          }
        }

        if (shouldReplace) {
          it.value() = {filePath, version, isDev};
        }
      }
    }
  }

  QSet<QString> loadedFiles;

  for (const auto &info : bestPlugins) {
    if (loadedFiles.contains(info.filePath)) {
      continue;
    }
    loadedFiles.insert(info.filePath);

    QPluginLoader pluginLoader(info.filePath);
    QObject *plugin = pluginLoader.instance();
    if (plugin) {
      // Set parent to Engine to ensure it is deleted when Engine is deleted
      plugin->setParent(this);
      Connector *connector = qobject_cast<Connector *>(plugin);
      if (connector) {
        if (!m_connectors.contains(connector->getId())) {
          qDebug() << "Loaded connector plugin:" << connector->getName()
                   << "from" << info.filePath;
          m_connectors.insert(connector->getId(), connector);
          // Connect to its signals via QObject cast
          connect(
              plugin,
              SIGNAL(dispatchFinished(QString, bool, QString, QJsonObject)),
              this,
              SLOT(onDispatchFinished(QString, bool, QString, QJsonObject)));
          // Keep backwards compatibility for older/unmodified connectors
          connect(plugin, SIGNAL(dispatchFinished(QString, bool, QString)),
                  this, SLOT(onDispatchFinished(QString, bool, QString)));
        } else {
          // Already loaded this connector
          plugin->setParent(nullptr); // clear parent before unload/delete
          pluginLoader.unload();
          delete plugin;
        }
      } else {
        qWarning() << "Plugin" << info.filePath << "is not a Connector.";
        plugin->setParent(nullptr);
        pluginLoader.unload();
        delete plugin;
      }
    } else {
      if (QLibrary::isLibrary(info.filePath)) {
        qWarning() << "Failed to load plugin" << info.filePath << ":"
                   << pluginLoader.errorString();
      }
    }
  }
}

Connector *Engine::getConnector(const QString &id) const {
  return m_connectors.value(id, nullptr);
}

QStringList Engine::getAvailableConnectors() const {
  QStringList active;
  for (auto it = m_connectors.constBegin(); it != m_connectors.constEnd();
       ++it) {
    if (it.value()->isEnabled()) {
      active << it.key();
    }
  }
  return active;
}

QStringList Engine::getAllConnectors() const { return m_connectors.keys(); }

void Engine::start() {
  if (!m_paused) {
    m_timer->start();
    qDebug() << "Engine started.";
  }
}

void Engine::stop() {
  m_timer->stop();
  qDebug() << "Engine stopped.";
}

void Engine::setPaused(bool paused) {
  if (m_paused == paused) {
    return;
  }

  m_paused = paused;
  if (m_paused) {
    m_timer->stop();
    qDebug() << "Engine paused.";
  } else {
    m_timer->start();
    qDebug() << "Engine resumed.";
  }
}

bool Engine::isPaused() const { return m_paused; }

void Engine::processQueue() {
  if (m_paused)
    return;

  auto items =
      m_storage->loadItemsByStates({ItemState::Queued, ItemState::Scheduled});

  QSettings settings;
  int autoArchiveDays = settings.value("autoArchiveDays", 0).toInt();
  if (autoArchiveDays > 0) {
    auto doneItems = m_storage->loadItemsByStates({ItemState::Done});
    std::vector<Item> itemsToArchive;
    QDateTime threshold =
        QDateTime::currentDateTime().addDays(-autoArchiveDays);

    for (auto &item : doneItems) {
      if (!item.metadata["lastDispatchTime"].toString().isEmpty()) {
        QDateTime lastDispatch = QDateTime::fromString(
            item.metadata["lastDispatchTime"].toString(), Qt::ISODate);
        if (lastDispatch.isValid() && lastDispatch < threshold) {
          item.state = ItemState::Archived;
          item.addHistory(QString("Auto-archived after %1 days in Done state.")
                              .arg(autoArchiveDays));
          itemsToArchive.push_back(item);
        }
      }
    }
    if (!itemsToArchive.empty()) {
      m_storage->saveItems(itemsToArchive);
    }
  }

  for (auto &item : items) {
    if (item.state == ItemState::Queued) {
      dispatchItem(item);
      // Process one at a time per tick
      break;
    }

    // Handle Scheduled items -> move to Queue if time reached
    if (item.state == ItemState::Scheduled) {
      if (item.scheduledTime.isValid() &&
          item.scheduledTime <= QDateTime::currentDateTime()) {
        qDebug() << "Scheduled item due:" << item.id;
        item.state = ItemState::Queued;
        item.addHistory("Scheduled time reached, moved to Queued.");
        m_storage->saveItem(item);
        // Will be picked up next tick
      }
    }
  }
}

void Engine::dispatchItem(Item &item) {
  qDebug() << "Dispatching item:" << item.id << "Source:" << item.sourcePath;

  Connector *connector = nullptr;
  if (!item.connectorId.isEmpty() && m_connectors.contains(item.connectorId)) {
    connector = m_connectors[item.connectorId];
  } else {
    // Fallback to qBittorrent if connectorId is not found or is "Default"
    // In AddItemDialog we add "Default" and "qBittorrent".
    // If "Default" is selected, we use qBittorrent for now.
    if (m_connectors.contains(Constants::QBittorrentConnectorId)) {
      connector = m_connectors[Constants::QBittorrentConnectorId];
    }
  }

  if (connector) {
    connector->dispatch(item);
  } else {
    qWarning() << "No connector found for item:" << item.id
               << " ConnectorId:" << item.connectorId;
    // Directly fail it here because we can't dispatch
    onDispatchFinished(item.id, false, "No suitable connector found.");
  }
}

void Engine::onDispatchFinished(const QString &itemId, bool success,
                                const QString &message,
                                const QJsonObject &metadata) {
  auto itemOpt = m_storage->loadItem(itemId);
  if (!itemOpt) {
    qWarning() << "Finished dispatch for unknown item:" << itemId;
    return;
  }

  Item item = *itemOpt;

  // Update metadata
  QJsonObject meta = item.metadata;
  meta["lastDispatchTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
  meta["dispatchResult"] = message;

  // Merge extra metadata provided by the connector
  for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
    meta.insert(it.key(), it.value());
  }

  if (success) {
    if (meta["delete_once_submitted"].toBool(false)) {
      item.addHistory(
          QString(
              "Item dispatched successfully to %1 and deleted as requested.")
              .arg(item.connectorId));
      qDebug() << "Item dispatched successfully and deleted as requested:"
               << itemId;
      m_storage->deleteItem(item.id);
      return;
    } else {
      item.state = ItemState::Done;
      qDebug() << "Item dispatched successfully:" << itemId;
    }
  } else {
    item.state = ItemState::Failed;
    meta["error"] = message;
    qWarning() << "Item dispatch failed:" << itemId << "Reason:" << message;
  }
  item.metadata = meta;

  m_storage->saveItem(item);
}
