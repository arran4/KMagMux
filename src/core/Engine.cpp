#include "Engine.h"
#include "QBittorrentConnector.h"
#include <QDebug>

Engine::Engine(StorageManager *storage, QObject *parent)
    : QObject(parent), m_storage(storage), m_paused(false) {
  m_timer = new QTimer(this);
  // Check queue every 5 seconds (configurable later)
  m_timer->setInterval(5000);
  connect(m_timer, &QTimer::timeout, this, &Engine::processQueue);

  // Register qBittorrent connector
  QBittorrentConnector *qb = new QBittorrentConnector(this);
  m_connectors.insert("qBittorrent", qb);
  connect(qb, &Connector::dispatchFinished, this, &Engine::onDispatchFinished);
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
    // Fallback to qBittorrent if connectorId is not found or is "Default"
    // In AddItemDialog we add "Default" and "qBittorrent".
    // If "Default" is selected, we use qBittorrent for now.
    if (m_connectors.contains("qBittorrent")) {
      connector = m_connectors["qBittorrent"];
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
