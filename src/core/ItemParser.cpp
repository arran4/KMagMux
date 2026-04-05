#include "ItemParser.h"
#include <QDateTime>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

std::vector<Item> ItemParser::parseLines(const QStringList &lines) {
  std::vector<Item> parsedItems;
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  int idx = 0;

  auto processLine = [&](QString line) {
    line = line.trimmed();
    if (line.isEmpty()) {
      return;
    }

    QString pathToCheck = line;
    if (line.startsWith("file://")) {
      const int filePrefixLen = 7;
      pathToCheck = line.mid(filePrefixLen);
    }

    Item newItem;
    newItem.id = QString::number(now) + "_" + QString::number(idx++) + "_item";
    newItem.state = ItemState::Unprocessed;
    newItem.sourcePath = pathToCheck;
    newItem.createdTime = QDateTime::currentDateTime();
    parsedItems.push_back(newItem);
  };

  for (const QString &line : lines) {
    processLine(line);
  }

  return parsedItems;
}
