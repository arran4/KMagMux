#ifndef ITEMPARSER_H
#define ITEMPARSER_H

#include "Item.h"
#include <QStringList>
#include <vector>

class ItemParser
{
public:
    static std::vector<Item> parseLines(const QStringList &lines);
};

#endif // ITEMPARSER_H
