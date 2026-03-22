import sys

with open('src/core/Item.h', 'r') as f:
    content = f.read()

if 'void addHistory(const QString &message);' not in content:
    content = content.replace('QJsonObject metadata; // Flexible metadata storage', 'QJsonObject metadata; // Flexible metadata storage\n\n  void addHistory(const QString &message);')
    with open('src/core/Item.h', 'w') as f:
        f.write(content)

with open('src/core/Item.cpp', 'r') as f:
    content = f.read()

if 'Item::addHistory' not in content:
    content += '''

#include <QJsonArray>

void Item::addHistory(const QString &message) {
  QJsonArray history = metadata["history"].toArray();
  QJsonObject entry;
  entry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
  entry["message"] = message;
  history.append(entry);
  metadata["history"] = history;
}
'''
    with open('src/core/Item.cpp', 'w') as f:
        f.write(content)
