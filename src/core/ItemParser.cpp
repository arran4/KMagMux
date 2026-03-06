#include "ItemParser.h"
#include <QDateTime>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

std::vector<Item> ItemParser::parseLines(const QStringList &lines) {
  std::vector<Item> parsedItems;
  qint64 now = QDateTime::currentMSecsSinceEpoch();
  int idx = 0;

  auto processLine = [&](QString line) {
    line = line.trimmed();
    if (line.isEmpty())
      return;

    QString pathToCheck = line;
    if (line.startsWith("file://")) {
      pathToCheck = line.mid(7);
    }

    Item newItem;
    newItem.id = QString::number(now) + "_" + QString::number(idx++) + "_item";
    newItem.state = ItemState::Unprocessed;
    newItem.sourcePath = line;
    newItem.createdTime = QDateTime::currentDateTime();
    parsedItems.push_back(newItem);
  };

  for (const QString &line : lines) {
    processLine(line);
  }

  return parsedItems;
}
