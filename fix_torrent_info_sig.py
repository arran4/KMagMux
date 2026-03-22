import sys

with open('src/ui/TorrentInfoDialog.h', 'r') as f:
    content = f.read()

# I need to change the constructor signature and store the item.
if 'const Item *m_item;' not in content:
    content = content.replace('QString m_sourcePath;', 'QString m_sourcePath;\n  const Item *m_item;')
    with open('src/ui/TorrentInfoDialog.h', 'w') as f:
        f.write(content)

with open('src/ui/TorrentInfoDialog.cpp', 'r') as f:
    content = f.read()

target_constructor = '''TorrentInfoDialog::TorrentInfoDialog(const QString &sourcePath, const Item *item, QWidget *parent)
    : QDialog(parent), m_sourcePath(sourcePath), m_isQuerying(false),
      m_currentTrackerIndex(0) {'''

replacement_constructor = '''TorrentInfoDialog::TorrentInfoDialog(const QString &sourcePath, const Item *item, QWidget *parent)
    : QDialog(parent), m_sourcePath(sourcePath), m_item(item), m_isQuerying(false),
      m_currentTrackerIndex(0) {'''

if 'm_item(item)' not in content:
    content = content.replace(target_constructor, replacement_constructor)
    with open('src/ui/TorrentInfoDialog.cpp', 'w') as f:
        f.write(content)

# Update setupUi to use m_item instead of item, because setupUi doesn't take parameters.
with open('src/ui/TorrentInfoDialog.cpp', 'r') as f:
    content = f.read()

target_setup = 'if (item && item->metadata.contains("history")) {'
replacement_setup = 'if (m_item && m_item->metadata.contains("history")) {'
if target_setup in content:
    content = content.replace(target_setup, replacement_setup)
    content = content.replace('item->metadata["history"]', 'm_item->metadata["history"]')
    with open('src/ui/TorrentInfoDialog.cpp', 'w') as f:
        f.write(content)
