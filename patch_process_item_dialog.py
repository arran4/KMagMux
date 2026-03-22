import sys

with open('src/ui/ProcessItemDialog.cpp', 'r') as f:
    content = f.read()

# Add a checkbox column "Delete when done" to m_itemsTable
content = content.replace('{"Enable", "Delete file", "Name", "Link"}', '{"Enable", "Delete file", "Delete when done", "Name", "Link"}')
content = content.replace('m_itemsTable->setColumnCount(4);', 'm_itemsTable->setColumnCount(5);')
content = content.replace('m_itemsTable->horizontalHeaderItem(1)->setToolTip("Delete file after import");',
'''m_itemsTable->horizontalHeaderItem(1)->setToolTip("Delete file after import");
  m_itemsTable->horizontalHeaderItem(2)->setToolTip("Delete item automatically after successful dispatch");''')

content = content.replace('m_itemsTable->setItem(i, 2, nameItem);', 'm_itemsTable->setItem(i, 3, nameItem);')
content = content.replace('m_itemsTable->setCellWidget(i, 3, linkLabel);', 'm_itemsTable->setCellWidget(i, 4, linkLabel);')
content = content.replace('m_itemsTable->setItem(i, 3, linkItem);', 'm_itemsTable->setItem(i, 4, linkItem);')

# Insert the "Delete when done" checkbox
insert_idx = content.find('QString displayName = m_items[i].getDisplayName();')
if insert_idx != -1:
    content = content[:insert_idx] + '''
    QTableWidgetItem *deleteWhenDoneItem = new QTableWidgetItem();
    deleteWhenDoneItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    deleteWhenDoneItem->setCheckState(Qt::Unchecked);
    m_itemsTable->setItem(i, 2, deleteWhenDoneItem);

    ''' + content[insert_idx:]

# Handle metadata inside getItems/onProcessClicked
meta_insert = '''
      QTableWidgetItem *deleteWhenDoneItem = m_itemsTable->item(i, 2);
      if (deleteWhenDoneItem && deleteWhenDoneItem->flags() & Qt::ItemIsUserCheckable) {
        if (deleteWhenDoneItem->checkState() == Qt::Checked) {
          QJsonObject meta = item.metadata;
          meta["delete_once_submitted"] = true;
          item.metadata = meta;
        }
      }
'''
content = content.replace('processedItems.push_back(item);', meta_insert + '\n      processedItems.push_back(item);')

with open('src/ui/ProcessItemDialog.cpp', 'w') as f:
    f.write(content)
