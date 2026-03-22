import sys

with open('src/ui/MainWindow.h', 'r') as f:
    content = f.read()

if 'QAction *m_infoAction;' not in content:
    content = content.replace('QAction *m_deleteAction;', 'QAction *m_deleteAction;\n  QAction *m_infoAction;')
    with open('src/ui/MainWindow.h', 'w') as f:
        f.write(content)

with open('src/ui/MainWindow.cpp', 'r') as f:
    content = f.read()

if 'm_infoAction(nullptr)' not in content:
    content = content.replace('m_deleteAction(nullptr),', 'm_deleteAction(nullptr), m_infoAction(nullptr),')

setup_actions_target = 'm_deleteAction = new QAction(QIcon::fromTheme("edit-delete"), tr("&Delete"), this);'
if 'm_infoAction = new QAction' not in content:
    info_action_init = '''
  m_infoAction = new QAction(QIcon::fromTheme("document-properties"), tr("&Get Info / History"), this);
  connect(m_infoAction, &QAction::triggered, this, [this]() {
    QTableView *view = getCurrentView();
    if (!view) return;
    const ItemModel *model = getCurrentModel();
    if (!model) return;
    ItemFilterProxyModel *proxy = qobject_cast<ItemFilterProxyModel *>(view->model());
    if (!proxy) return;
    QModelIndexList selection = view->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;
    QModelIndex sourceIndex = proxy->mapToSource(selection.first());
    Item item = model->getItem(sourceIndex.row());
    QString sourcePath = item.sourcePath;
    if (sourcePath.startsWith("magnet:") || sourcePath.endsWith(".torrent")) {
      TorrentInfoDialog dialog(sourcePath, this);
      dialog.exec();
    } else {
      QMessageBox::information(this, "Item Information", QString("Source Path:\n%1").arg(sourcePath));
    }
  });
  actionCollection()->addAction("info_item", m_infoAction);
'''
    content = content.replace(setup_actions_target, setup_actions_target + info_action_init)

menu_target = '''
  menu.addAction(m_archiveAction);
  if (view == m_doneView || view == m_errorView) {
    menu.addAction(m_archiveAllAction);
  }
  menu.addSeparator();
  menu.addAction(m_deleteAction);

  menu.exec(view->viewport()->mapToGlobal(pos));
'''
menu_replace = '''
  menu.addAction(m_archiveAction);
  if (view == m_doneView || view == m_errorView) {
    menu.addAction(m_archiveAllAction);
  }
  menu.addSeparator();
  menu.addAction(m_infoAction);
  menu.addAction(m_deleteAction);

  menu.exec(view->viewport()->mapToGlobal(pos));
'''
if 'menu.addAction(m_infoAction);' not in content:
    content = content.replace(menu_target, menu_replace)

update_actions_target = '''
  if (m_deleteAction)
    m_deleteAction->setEnabled(hasSelection);
'''
update_actions_replace = '''
  if (m_deleteAction)
    m_deleteAction->setEnabled(hasSelection);
  if (m_infoAction)
    m_infoAction->setEnabled(hasSelection && selection.size() == 1);
'''
if 'm_infoAction->setEnabled' not in content:
    content = content.replace(update_actions_target, update_actions_replace)

if 'if (m_infoAction) m_infoAction->setEnabled(false);' not in content:
    content = content.replace('if (m_deleteAction) m_deleteAction->setEnabled(false);', 'if (m_deleteAction) m_deleteAction->setEnabled(false);\n    if (m_infoAction) m_infoAction->setEnabled(false);')

# One important thing for TorrentInfoDialog. The signature is `TorrentInfoDialog(QString sourcePath, QWidget* parent)`.
# But wait, I added history array parsing to it. How does it get the `Item`?
# Ah! Look at my `patch_info_history.py`!
# I used `item.metadata["history"]` inside `TorrentInfoDialog`. Let's see how I did it.

with open('src/ui/MainWindow.cpp', 'w') as f:
    f.write(content)
