import sys

with open('src/ui/TorrentInfoDialog.cpp', 'r') as f:
    content = f.read()

target = 'QJsonArray history = item.metadata["history"].toArray();'
if target in content:
    # `item` is not available in TorrentInfoDialog which is created with just a string path!
    print("Found the target! I need to change TorrentInfoDialog to take an Item.")
