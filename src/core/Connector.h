#ifndef CONNECTOR_H
#define CONNECTOR_H

#include <QObject>
#include <QString>
#include "Item.h"

class Connector : public QObject {
    Q_OBJECT

public:
    explicit Connector(QObject *parent = nullptr);
    virtual ~Connector();

    virtual QString getId() const = 0;
    virtual QString getName() const = 0;
    virtual void dispatch(const Item &item) = 0;

signals:
    void dispatchFinished(const QString &itemId, bool success, const QString &message);
};

#endif // CONNECTOR_H
