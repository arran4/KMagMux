#ifndef ENGINE_H
#define ENGINE_H

#include "Connector.h"
#include "StorageManager.h"
#include <QMap>
#include <QObject>
#include <QStringList>
#include <QTimer>

class Engine : public QObject {
  Q_OBJECT

public:
  explicit Engine(StorageManager *storage, QObject *parent = nullptr);
  void start();
  void stop();
  QStringList getAvailableConnectors() const;

  QString defaultConnectorId() const { return m_defaultConnectorId; }
  void setDefaultConnectorId(const QString &id) { m_defaultConnectorId = id; }

private slots:
  void processQueue();
  void onDispatchFinished(const QString &itemId, bool success,
                          const QString &message);

private:
  StorageManager *m_storage;
  QTimer *m_timer;
  bool m_paused;
  QMap<QString, Connector *> m_connectors;
  QString m_defaultConnectorId{"qBittorrent"};

  void dispatchItem(Item &item);
};

#endif // ENGINE_H
