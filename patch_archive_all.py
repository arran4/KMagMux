import sys

with open('src/ui/MainWindow.cpp', 'r') as f:
    content = f.read()

# 1. Add m_archiveAllAction in MainWindow class
if 'QAction *m_archiveAllAction;' not in content:
    with open('src/ui/MainWindow.h', 'r') as f_h:
        h_content = f_h.read()
    h_content = h_content.replace('QAction *m_archiveAction;', 'QAction *m_archiveAction;\n  QAction *m_archiveAllAction;')
    with open('src/ui/MainWindow.h', 'w') as f_h:
        f_h.write(h_content)

# 2. Add m_archiveAllAction initialization
if 'm_archiveAllAction(nullptr)' not in content:
    content = content.replace('m_archiveAction(nullptr),', 'm_archiveAction(nullptr), m_archiveAllAction(nullptr),')

# 3. Create m_archiveAllAction and connect it
setup_actions_target = 'm_archiveAction = new QAction(tr("&Archive"), this);'
if 'm_archiveAllAction =' not in content:
    archive_all_init = '''
  m_archiveAllAction = new QAction(tr("Archive &All"), this);
  connect(m_archiveAllAction, &QAction::triggered, this, [this]() {
    QTableView *view = getCurrentView();
    if (!view) return;
    const ItemModel *model = getCurrentModel();
    if (!model) return;

    std::vector<Item> itemsToSave;
    itemsToSave.reserve(model->rowCount());
    for (int i = 0; i < model->rowCount(); ++i) {
      Item item = model->getItem(i);
      item.state = ItemState::Archived;
      itemsToSave.push_back(item);
    }
    if (!itemsToSave.empty()) {
      m_storage->saveItems(itemsToSave);
    }
  });
  actionCollection()->addAction("archive_all_items", m_archiveAllAction);
'''
    content = content.replace(setup_actions_target, setup_actions_target + archive_all_init)

# 4. Add to context menu if view is m_doneView or m_errorView
menu_target = '''
  menu.addAction(m_archiveAction);
  menu.addSeparator();
  menu.addAction(m_deleteAction);
'''

menu_replace = '''
  menu.addAction(m_archiveAction);
  if (view == m_doneView || view == m_errorView) {
    menu.addAction(m_archiveAllAction);
  }
  menu.addSeparator();
  menu.addAction(m_deleteAction);
'''
content = content.replace(menu_target, menu_replace)

# 5. Update actions state
update_actions_target = '''
  if (m_archiveAction)
    m_archiveAction->setEnabled(hasSelection);
'''
update_actions_replace = '''
  if (m_archiveAction)
    m_archiveAction->setEnabled(hasSelection);
  if (m_archiveAllAction) {
    const ItemModel *model = getCurrentModel();
    m_archiveAllAction->setVisible(view == m_doneView || view == m_errorView);
    m_archiveAllAction->setEnabled(model && model->rowCount() > 0 && (view == m_doneView || view == m_errorView));
  }
'''
content = content.replace(update_actions_target, update_actions_replace)

if 'm_archiveAllAction->setEnabled(false);' not in content:
    content = content.replace('if (m_archiveAction) m_archiveAction->setEnabled(false);',
                              'if (m_archiveAction) m_archiveAction->setEnabled(false);\n    if (m_archiveAllAction) m_archiveAllAction->setEnabled(false);')

with open('src/ui/MainWindow.cpp', 'w') as f:
    f.write(content)
