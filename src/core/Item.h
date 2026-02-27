#ifndef ITEM_H
#define ITEM_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QJsonDocument>

enum class ItemState {
    Unprocessed,
    Queued,
    Scheduled,
    Held,
    Dispatched,
    Failed,
    Archived
};

struct Item {
    QString id;
    ItemState state;
    QString sourcePath; // Original path or magnet link
    QString destinationPath;
    QString connectorId;
    QDateTime createdTime;
    QDateTime scheduledTime;
    QJsonObject metadata; // Flexible metadata storage

    // Serialization helpers
    QJsonObject toJson() const;
    static Item fromJson(const QJsonObject &json);

    // Helper to get string representation of state
    QString stateToString() const;
    static ItemState stringToState(const QString &s);
};

Q_DECLARE_METATYPE(Item)

#endif // ITEM_H
