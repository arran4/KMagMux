import os

filepath = 'src/plugins/qbittorrent/QBittorrentConnector.cpp'
with open(filepath, 'r') as f:
    content = f.read()

new_content = content.replace(
    'QBittorrentConnector::QBittorrentConnector() : QBittorrentConnector(nullptr) {}',
    '''QBittorrentConnector::QBittorrentConnector() : QBittorrentConnector(nullptr) {}'''
)

new_content = new_content.replace(
    'm_isPending(false) {}',
    '''m_isPending(false) {
  QSettings settings;
  settings.beginGroup("Plugins/qBittorrent");
  m_baseUrl = settings.value("baseUrl", "http://localhost:8080").toString();
  m_username = settings.value("username", "").toString();
  m_password = settings.value("password", "").toString();
  settings.endGroup();
}'''
)

with open(filepath, 'w') as f:
    f.write(new_content)

print("Patched QBittorrentConnector.cpp initialization")
