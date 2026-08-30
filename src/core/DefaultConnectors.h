#ifndef DEFAULTCONNECTORS_H
#define DEFAULTCONNECTORS_H

#include <QStringList>

class DefaultConnectors {
public:
  static QStringList get(const QStringList &availableConnectors);
  static void set(const QStringList &connectors);
};

#endif // DEFAULTCONNECTORS_H
