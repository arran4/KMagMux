import sys

with open('src/ui/MainWindow.cpp', 'r') as f:
    content = f.read()

# Replace Item state changes in MainWindow.cpp with addHistory
target_on_item_action = '''
  qDebug() << "Changing item state for:" << item.id << "to" << (int)newState;

  item.state = newState;
  if (m_storage->saveItem(item)) {
'''
replacement_on_item_action = '''
  qDebug() << "Changing item state for:" << item.id << "to" << (int)newState;

  QString oldState = item.stateToString();
  item.state = newState;
  item.addHistory(QString("State changed from %1 to %2 by user action.").arg(oldState, item.stateToString()));

  if (m_storage->saveItem(item)) {
'''
if 'item.addHistory' not in content:
    content = content.replace(target_on_item_action, replacement_on_item_action)

target_archive_all = '''
    for (int i = 0; i < model->rowCount(); ++i) {
      Item item = model->getItem(i);
      item.state = ItemState::Archived;
      itemsToSave.push_back(item);
    }
'''
replacement_archive_all = '''
    for (int i = 0; i < model->rowCount(); ++i) {
      Item item = model->getItem(i);
      QString oldState = item.stateToString();
      item.state = ItemState::Archived;
      item.addHistory(QString("State changed from %1 to Archived via Bulk Archive.").arg(oldState));
      itemsToSave.push_back(item);
    }
'''
if 'item.addHistory' not in content[content.find('m_archiveAllAction'):]:
    content = content.replace(target_archive_all, replacement_archive_all)

target_add_items = '''
  for (const QString &line : lines) {
    if (line.trimmed().isEmpty())
      continue;

    Item item;
'''
replacement_add_items = '''
  for (const QString &line : lines) {
    if (line.trimmed().isEmpty())
      continue;

    Item item;
'''
# We don't need to add history on creation necessarily, but it's nice.
# Actually ItemParser creates items. Let's look at ProcessItemDialog.

with open('src/ui/MainWindow.cpp', 'w') as f:
    f.write(content)

with open('src/ui/ProcessItemDialog.cpp', 'r') as f:
    content = f.read()

target_process_items = '''
      Item item = m_items[i];
      item.state = selectedState;
      item.connectorId = m_connectorCombo->currentText();
'''
replacement_process_items = '''
      Item item = m_items[i];
      QString oldState = item.stateToString();
      item.state = selectedState;
      item.addHistory(QString("Processed and state set to %1").arg(item.stateToString()));
      item.connectorId = m_connectorCombo->currentText();
'''
if 'item.addHistory' not in content:
    content = content.replace(target_process_items, replacement_process_items)
    with open('src/ui/ProcessItemDialog.cpp', 'w') as f:
        f.write(content)


with open('src/core/Engine.cpp', 'r') as f:
    content = f.read()

target_dispatch_finish = '''
  if (success) {
    if (meta["delete_once_submitted"].toBool(false)) {
'''
replacement_dispatch_finish = '''
  if (success) {
    if (meta["delete_once_submitted"].toBool(false)) {
      item.addHistory(QString("Item dispatched successfully to %1 and deleted as requested.").arg(item.connectorId));
'''
if 'item.addHistory' not in content:
    content = content.replace(target_dispatch_finish, replacement_dispatch_finish)

target_dispatch_finish_done = '''
    } else {
      item.state = ItemState::Done;
      qDebug() << "Item dispatched successfully:" << itemId;
    }
  } else {
    item.state = ItemState::Failed;
'''
replacement_dispatch_finish_done = '''
    } else {
      item.state = ItemState::Done;
      item.addHistory(QString("Item dispatched successfully to %1.").arg(item.connectorId));
      qDebug() << "Item dispatched successfully:" << itemId;
    }
  } else {
    item.state = ItemState::Failed;
    item.addHistory(QString("Item dispatch failed. Reason: %1").arg(message));
'''
if 'item.addHistory(QString("Item dispatched successfully' not in content:
    content = content.replace(target_dispatch_finish_done, replacement_dispatch_finish_done)

target_scheduled = '''
    if (item.state == ItemState::Scheduled) {
      if (item.scheduledTime.isValid() &&
          item.scheduledTime <= QDateTime::currentDateTime()) {
        qDebug() << "Scheduled item due:" << item.id;
        item.state = ItemState::Queued;
        m_storage->saveItem(item);
'''
replacement_scheduled = '''
    if (item.state == ItemState::Scheduled) {
      if (item.scheduledTime.isValid() &&
          item.scheduledTime <= QDateTime::currentDateTime()) {
        qDebug() << "Scheduled item due:" << item.id;
        item.state = ItemState::Queued;
        item.addHistory("Scheduled time reached, moved to Queued.");
        m_storage->saveItem(item);
'''
if 'item.addHistory("Scheduled time reached' not in content:
    content = content.replace(target_scheduled, replacement_scheduled)

with open('src/core/Engine.cpp', 'w') as f:
    f.write(content)
