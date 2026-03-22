import sys

with open('src/ui/MainWindow.cpp', 'r') as f:
    content = f.read()

target = '''
    for (int i = 0; i < model->rowCount(); ++i) {
      Item item = model->getItem(i);
      item.state = ItemState::Archived;
      itemsToSave.push_back(item);
    }
'''
replacement = '''
    for (int i = 0; i < model->rowCount(); ++i) {
      Item item = model->getItem(i);
      QString oldState = item.stateToString();
      item.state = ItemState::Archived;
      item.addHistory(QString("State changed from %1 to Archived via Bulk Archive.").arg(oldState));
      itemsToSave.push_back(item);
    }
'''
if 'item.addHistory(QString("State changed from %1 to Archived via Bulk Archive.").arg(oldState));' not in content:
    content = content.replace(target, replacement)
    with open('src/ui/MainWindow.cpp', 'w') as f:
        f.write(content)
