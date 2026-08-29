#ifndef DEFAULTCONNECTORS_H
#define DEFAULTCONNECTORS_H

#include <QStringList>

class DefaultConnectors {
public:
  // Reads and calculates the effective default connectors given a list of
  // currently available/enabled connectors.
  static QStringList get(const QStringList &availableConnectors);

  // Saves the explicit user choice of default connectors.
  static void set(const QStringList &connectors);
};

#endif // DEFAULTCONNECTORS_H
