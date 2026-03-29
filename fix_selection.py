import sys

with open('src/ui/MainWindow.cpp', 'r') as f:
    content = f.read()

target = '''
  if (m_infoAction)
    QModelIndexList selection = view->selectionModel()->selectedRows();
    m_infoAction->setEnabled(hasSelection && selection.size() == 1);
'''
replacement = '''
  if (m_infoAction) {
    QModelIndexList selection = view->selectionModel()->selectedRows();
    m_infoAction->setEnabled(hasSelection && selection.size() == 1);
  }
'''
if target in content:
    content = content.replace(target, replacement)
    with open('src/ui/MainWindow.cpp', 'w') as f:
        f.write(content)
