#include "Engine.h"
#include "Constants.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QLibrary>
#include <QPluginLoader>

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

  QSet<QString> loadedFileNames;

  for (const QString &path : pluginPaths) {
    QDir pluginsDir(path);
    if (!pluginsDir.exists()) {
      continue;
    }

    qDebug() << "Looking for plugins in:" << pluginsDir.absolutePath();

    for (QString fileName : pluginsDir.entryList(QDir::Files)) {
      if (loadedFileNames.contains(fileName)) {
        continue;
      }

      QPluginLoader pluginLoader(pluginsDir.absoluteFilePath(fileName));

      // Check metadata before instantiating to avoid loading non-plugin
      // libraries blockingly
      QJsonObject meta = pluginLoader.metaData();
      if (meta.value("IID").toString() != "com.kmagmux.Connector/1.0") {
        continue;
      }

      QObject *plugin = pluginLoader.instance();
      if (plugin) {
        Connector *connector = qobject_cast<Connector *>(plugin);
        if (connector) {
          if (!m_connectors.contains(connector->getId())) {
            qDebug() << "Loaded connector plugin:" << connector->getName()
                     << "from" << pluginsDir.absolutePath();
            m_connectors.insert(connector->getId(), connector);
            loadedFileNames.insert(fileName);
            // Connect to its signals via QObject cast
            connect(plugin, SIGNAL(dispatchFinished(QString, bool, QString)),
                    this, SLOT(onDispatchFinished(QString, bool, QString)));
          }
        } else {
          qWarning() << "Plugin" << fileName << "is not a Connector.";
        }
      } else {
        // Since we iterate through everything, it might fail to load non-plugin
        // files but we only warn if it really is a library that failed.
        if (QLibrary::isLibrary(fileName)) {
          qWarning() << "Failed to load plugin" << fileName << ":"
                     << pluginLoader.errorString();
        }
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
                                const QString &message) {
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

  if (success) {
    item.state = ItemState::Done;
    qDebug() << "Item dispatched successfully:" << itemId;
  } else {
    item.state = ItemState::Failed;
    meta["error"] = message;
    qWarning() << "Item dispatch failed:" << itemId << "Reason:" << message;
  }
  item.metadata = meta;

  m_storage->saveItem(item);
}
