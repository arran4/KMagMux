#ifndef ITEMPARSER_H
#define ITEMPARSER_H

#include "Item.h"
#include <QString>
#include <QStringList>
#include <vector>

struct RejectedInput {
  QString input;
  QString reason;
};

struct ParseResult {
  std::vector<Item> items;
  std::vector<RejectedInput> rejectedInputs;
};

class ItemParser {
public:
  static ParseResult parseLines(const QStringList &lines);
};

#endif // ITEMPARSER_H
