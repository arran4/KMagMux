#include "Engine.h"
#include <QDebug>

Engine::Engine(StorageManager *storage, QObject *parent)
    : QObject(parent), m_storage(storage), m_paused(false) {
    m_timer = new QTimer(this);
    // Check queue every 5 seconds (configurable later)
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, &Engine::processQueue);
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
    if (m_paused) return;

    // In a real implementation, we'd query the storage for items in 'Queued' state
    // For now, load all (inefficient but works for MVP)
    auto items = m_storage->loadAllItems();

    for (auto &item : items) {
        if (item.state == ItemState::Queued) {
            // Process the item!
            dispatchItem(item);
            // Process one at a time per tick? Or all?
            // Let's break after one for now to simulate serial processing
            break;
        }

        // Handle Scheduled items -> move to Queue if time reached
        if (item.state == ItemState::Scheduled) {
            if (item.scheduledTime.isValid() && item.scheduledTime <= QDateTime::currentDateTime()) {
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

    // Here we would use the Connector interface to send to qBittorrent/etc.
    // For MVP, we simulate success after "dispatching".

    // Update state to Dispatched
    item.state = ItemState::Dispatched;

    // Update metadata with result
    QJsonObject meta = item.metadata;
    meta["lastDispatchTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    meta["dispatchResult"] = "Simulated Success";
    item.metadata = meta;

    if (m_storage->saveItem(item)) {
        qDebug() << "Item dispatched successfully:" << item.id;
    } else {
        qWarning() << "Failed to save dispatched state for:" << item.id;
    }
}
