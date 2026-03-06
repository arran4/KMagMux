import os

filepath = 'src/plugins/qbittorrent/QBittorrentConnector.h'
with open(filepath, 'r') as f:
    content = f.read()

new_content = content.replace(
    '  void setCredentials(const QString &username, const QString &password);',
    '''  void setCredentials(const QString &username, const QString &password);

  bool hasSettings() const override;
  QWidget* createSettingsWidget(QWidget* parent) override;
  void saveSettings(QWidget* settingsWidget) override;'''
)

with open(filepath, 'w') as f:
    f.write(new_content)

filepath = 'src/plugins/qbittorrent/QBittorrentConnector.cpp'
with open(filepath, 'r') as f:
    content = f.read()

new_content = content.replace(
    '#include <QUrlQuery>',
    '''#include <QUrlQuery>
#include <QSettings>
#include <QWidget>
#include <QFormLayout>
#include <QLineEdit>'''
)

new_content = new_content.replace(
    'm_baseUrl(""), m_username(""), m_password(""), m_pendingItem(),',
    '''m_baseUrl(""), m_username(""), m_password(""), m_pendingItem(),'''
)

with open(filepath, 'w') as f:
    f.write(new_content)

print("Patched QBittorrentConnector.h and QBittorrentConnector.cpp headers")
