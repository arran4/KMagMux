#include "DefaultConnectors.h"
#include "Constants.h"
#include <QSettings>

QStringList DefaultConnectors::get(const QStringList &availableConnectors) {
  QSettings settings;
  const QString key = "defaultConnectors";

  if (!settings.contains(key)) {
    if (availableConnectors.contains(Constants::QBittorrentConnectorId)) {
      return {Constants::QBittorrentConnectorId};
    }
    return {};
  }

  const QStringList configured = settings.value(key).toStringList();
  QStringList result;

  for (const QString &id : configured) {
    const QString normalizedId = (id == Constants::DefaultActionName) ? Constants::QBittorrentConnectorId : id;

    if (availableConnectors.contains(normalizedId) && !result.contains(normalizedId)) {
      result.append(normalizedId);
    }
  }

  return result;
}

void DefaultConnectors::set(const QStringList &connectors) {
  QSettings settings;
  QStringList canonical;

  for (const QString &id : connectors) {
    const QString normalizedId = (id == Constants::DefaultActionName) ? Constants::QBittorrentConnectorId : id;
    if (!canonical.contains(normalizedId)) {
      canonical.append(normalizedId);
    }
  }

  settings.setValue("defaultConnectors", canonical);
}
