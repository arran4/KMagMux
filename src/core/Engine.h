#ifndef ENGINE_H
#define ENGINE_H

#include <QObject>
#include <QTimer>
#include <QMap>
#include "StorageManager.h"
#include "Connector.h"

class Engine : public QObject {
    Q_OBJECT

public:
    explicit Engine(StorageManager *storage, QObject *parent = nullptr);
    void start();
    void stop();

private slots:
    void processQueue();
    void onDispatchFinished(const QString &itemId, bool success, const QString &message);

private:
    StorageManager *m_storage;
    QTimer *m_timer;
    bool m_paused;
    QMap<QString, Connector*> m_connectors;

    void dispatchItem(Item &item);
};

#endif // ENGINE_H
