#ifndef CONNECTOR_H
#define CONNECTOR_H

#include "HttpApiEndpoint.h"
#include "Item.h"
#include <QList>
#include <QMap>
#include <QString>

class Connector {
public:
  virtual ~Connector() = default;

  virtual QString getId() const = 0;
  virtual QString getName() const = 0;
  virtual void dispatch(const Item &item) = 0;

  virtual bool isEnabled() const { return true; }

  virtual bool hasSettings() const { return false; }
  virtual class QWidget *createSettingsWidget(class QWidget *parent) {
    return nullptr;
  }
  virtual void saveSettings(class QWidget *settingsWidget) {}

  virtual bool hasDebugMenu() const { return false; }
  virtual QList<HttpApiEndpoint> getHttpApiEndpoints() const {
    return QList<HttpApiEndpoint>();
  }
  virtual QMap<QString, QString> getApiSubstitutions() const {
    return QMap<QString, QString>();
  }
};

#define Connector_iid "com.kmagmux.Connector/1.0"
Q_DECLARE_INTERFACE(Connector, Connector_iid)

#endif // CONNECTOR_H
