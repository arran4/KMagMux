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

  bool isEnabled() const override {
      return m_enabled;
  }
  void setEnabled(bool enabled) { m_enabled = enabled; }

private:
  bool m_enabled = true;
};

#endif // MOCKPLUGIN_H
