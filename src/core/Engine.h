#ifndef ENGINE_H
#define ENGINE_H

#include <QObject>
#include <QTimer>
#include "StorageManager.h"

class Engine : public QObject {
    Q_OBJECT

public:
    explicit Engine(StorageManager *storage, QObject *parent = nullptr);
    void start();
    void stop();

private slots:
    void processQueue();

private:
    StorageManager *m_storage;
    QTimer *m_timer;
    bool m_paused;

    void dispatchItem(Item &item);
};

#endif // ENGINE_H
