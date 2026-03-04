#include "Engine.h"
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

  // Install path (relative to executable)
#ifdef KMAGMUX_REL_PLUGIN_DIR
  pluginPaths << QDir::cleanPath(appDir + "/" +
                                 QStringLiteral(KMAGMUX_REL_PLUGIN_DIR));
#endif

  for (const QString &path : pluginPaths) {
    QDir pluginsDir(path);
    if (!pluginsDir.exists()) {
      continue;
    }

    qDebug() << "Looking for plugins in:" << pluginsDir.absolutePath();

    for (QString fileName : pluginsDir.entryList(QDir::Files)) {
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
            // Connect to its signals via QObject cast
            connect(plugin, SIGNAL(dispatchFinished(QString, bool, QString)),
                    this, SLOT(onDispatchFinished(QString, bool, QString)));
          } else {
            // Already loaded this connector (e.g. from dev path instead of
            // install path)
            pluginLoader.unload();
          }
        } else {
          qWarning() << "Plugin" << fileName << "is not a Connector.";
          pluginLoader.unload();
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

QStringList Engine::getAvailableConnectors() const {
  return m_connectors.keys();
}

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

void Engine::processQueue() {
  if (m_paused)
    return;

  // In a real implementation, we'd query the storage for items in 'Queued'
  // state For now, load all (inefficient but works for MVP)
  auto items = m_storage->loadAllItems();

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
    // Fallback to default connector if connectorId is not found or is "Default"
    if (m_connectors.contains(m_defaultConnectorId)) {
      connector = m_connectors[m_defaultConnectorId];
    } else if (!m_connectors.isEmpty()) {
      // If the default is not available, pick the first available connector
      connector = m_connectors.first();
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
    item.state = ItemState::Dispatched;
    qDebug() << "Item dispatched successfully:" << itemId;
  } else {
    item.state = ItemState::Failed;
    meta["error"] = message;
    qWarning() << "Item dispatch failed:" << itemId << "Reason:" << message;
  }
  item.metadata = meta;

  m_storage->saveItem(item);
}
