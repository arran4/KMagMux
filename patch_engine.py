import os

filepath = 'src/core/Engine.h'
with open(filepath, 'r') as f:
    content = f.read()

new_content = content.replace(
    '  QStringList getAvailableConnectors() const;',
    '  QStringList getAvailableConnectors() const;\n  Connector* getConnector(const QString &id) const;'
)

with open(filepath, 'w') as f:
    f.write(new_content)

filepath = 'src/core/Engine.cpp'
with open(filepath, 'r') as f:
    content = f.read()

new_content = content.replace(
    'QStringList Engine::getAvailableConnectors() const {',
    '''Connector* Engine::getConnector(const QString &id) const {
  return m_connectors.value(id, nullptr);
}

QStringList Engine::getAvailableConnectors() const {'''
)

with open(filepath, 'w') as f:
    f.write(new_content)

print("Patched Engine")
