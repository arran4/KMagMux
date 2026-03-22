import sys

with open('src/core/Engine.cpp', 'r') as f:
    content = f.read()

# Replace onDispatchFinished logic to handle delete_once_submitted
target = '''
  if (success) {
    item.state = ItemState::Done;
    qDebug() << "Item dispatched successfully:" << itemId;
  } else {
    item.state = ItemState::Failed;
    meta["error"] = message;
    qWarning() << "Item dispatch failed:" << itemId << "Reason:" << message;
  }
  item.metadata = meta;

  m_storage->saveItem(item);
'''

replacement = '''
  if (success) {
    if (meta["delete_once_submitted"].toBool(false)) {
      qDebug() << "Item dispatched successfully and deleted as requested:" << itemId;
      m_storage->deleteItem(item.id);
      return;
    } else {
      item.state = ItemState::Done;
      qDebug() << "Item dispatched successfully:" << itemId;
    }
  } else {
    item.state = ItemState::Failed;
    meta["error"] = message;
    qWarning() << "Item dispatch failed:" << itemId << "Reason:" << message;
  }
  item.metadata = meta;

  m_storage->saveItem(item);
'''

if target in content:
    content = content.replace(target, replacement)
    with open('src/core/Engine.cpp', 'w') as f:
        f.write(content)
else:
    print("Could not find the target code in Engine.cpp")
