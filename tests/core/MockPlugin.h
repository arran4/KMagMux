#ifndef MOCKPLUGIN_H
#define MOCKPLUGIN_H

#include "core/Connector.h"
#include <QObject>
#include <QtPlugin>

class MockPlugin : public QObject, public Connector {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "com.kmagmux.Connector/1.0")
  Q_INTERFACES(Connector)

public:
  QString getId() const override { return "MockConnector"; }
  QString getName() const override { return "Mock Connector"; }
  void dispatch(const Item &item) override { Q_UNUSED(item); }
};

#endif // MOCKPLUGIN_H
