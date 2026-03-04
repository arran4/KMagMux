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

    // Check if it's a local file that might contain links (.txt or .html)
    QFileInfo fileInfo(pathToCheck);
    if (fileInfo.exists() && fileInfo.isFile()) {
      QString ext = fileInfo.suffix().toLower();
      if (ext == "txt" || ext == "html" || ext == "htm") {
        QFile file(pathToCheck);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
          QTextStream in(&file);
          while (!in.atEnd()) {
            QString fileLine = in.readLine().trimmed();
            if (fileLine.isEmpty())
              continue;

            if (ext == "html" || ext == "htm") {
              // simple naive regex to extract href
              QRegularExpression re("href=[\"']([^\"']+)[\"']");
              QRegularExpressionMatchIterator i = re.globalMatch(fileLine);
              while (i.hasNext()) {
                QRegularExpressionMatch match = i.next();
                QString link = match.captured(1).trimmed();
                if (!link.isEmpty()) {
                  Item newItem;
                  newItem.id = QString::number(now) + "_" +
                               QString::number(idx++) + "_item";
                  newItem.state = ItemState::Unprocessed;
                  newItem.sourcePath = link;
                  newItem.createdTime = QDateTime::currentDateTime();
                  parsedItems.push_back(newItem);
                }
              }
            } else {
              // txt file
              Item newItem;
              newItem.id =
                  QString::number(now) + "_" + QString::number(idx++) + "_item";
              newItem.state = ItemState::Unprocessed;
              newItem.sourcePath = fileLine;
              newItem.createdTime = QDateTime::currentDateTime();
              parsedItems.push_back(newItem);
            }
          }
        }
        return; // Skip adding the .txt/.html file itself as an item
      }
    }

    // Not a text/html file (or doesn't exist locally), treat as regular item
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
