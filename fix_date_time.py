import sys

with open('src/ui/TorrentInfoDialog.cpp', 'r') as f:
    content = f.read()

if '#include <QDateTime>' not in content:
    content = content.replace('#include <QFormLayout>', '#include <QDateTime>\n#include <QFormLayout>')
    with open('src/ui/TorrentInfoDialog.cpp', 'w') as f:
        f.write(content)
