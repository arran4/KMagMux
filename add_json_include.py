import sys

with open('src/ui/TorrentInfoDialog.cpp', 'r') as f:
    content = f.read()

if '#include <QJsonArray>' not in content:
    content = content.replace('#include "TorrentInfoDialog.h"', '#include "TorrentInfoDialog.h"\n#include <QJsonArray>\n#include <QJsonObject>')
    with open('src/ui/TorrentInfoDialog.cpp', 'w') as f:
        f.write(content)
