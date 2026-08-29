#include "DefaultConnectors.h"
#include "Constants.h"
#include <QSettings>

QStringList DefaultConnectors::get(const QStringList &availableConnectors) {
  QSettings settings;
  const QString key = "defaultConnectors";

  if (!settings.contains(key)) {
    // Absent setting fallback
    if (availableConnectors.contains(Constants::QBittorrentConnectorId)) {
      return {Constants::QBittorrentConnectorId};
    }
    return {};
  }

  // Explicitly configured values (could be explicitly empty)
  const QStringList configured = settings.value(key).toStringList();
  QStringList result;

  for (const QString &id : configured) {
    // Normalize legacy "Default"
    const QString normalizedId = (id == Constants::DefaultActionName)
                                     ? Constants::QBittorrentConnectorId
                                     : id;

    // Keep if available and not a duplicate
    if (availableConnectors.contains(normalizedId) &&
        !result.contains(normalizedId)) {
      result.append(normalizedId);
    }
  }

  return result;
}

void DefaultConnectors::set(const QStringList &connectors) {
  QSettings settings;
  QStringList canonical;

  for (const QString &id : connectors) {
    const QString normalizedId = (id == Constants::DefaultActionName)
                                     ? Constants::QBittorrentConnectorId
                                     : id;
    if (!canonical.contains(normalizedId)) {
      canonical.append(normalizedId);
    }
  }

  // Preserve explicitly empty list
  settings.setValue("defaultConnectors", canonical);
}
