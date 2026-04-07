#ifndef ENGINE_H
#define ENGINE_H

#include "Item.h"
#include <QMap>
#include <QObject>
#include <QStringList>
#include <QTimer>

class Engine : public QObject {
  Q_OBJECT

public:
  explicit Engine(StorageManager *storage, QObject *parent = nullptr);
  void start() const;
  static void stop();
  void setPaused(bool paused);
  bool isPaused() const;
  QStringList getAvailableConnectors() const;
  QStringList getAllConnectors() const;
  Connector *getConnector(const QString &identifier) const;

private slots:
  void processQueue();
  void onDispatchFinished(const QString &itemId, bool success,
                          const QString &message,
                          const QJsonObject &metadata = QJsonObject());

private:
  StorageManager *m_storage;
  QTimer *m_timer;
  bool m_paused;
  QMap<QString, Connector *> m_connectors;

  void dispatchItem(Item &item);
};

#endif // ENGINE_H
