import sys

with open('src/ui/MainWindow.h', 'r') as f:
    h_content = f.read()

if 'QAction *m_unprocessAction;' not in h_content:
    h_content = h_content.replace('QAction *m_reprocessAction;', 'QAction *m_reprocessAction;\n  QAction *m_unprocessAction;')
    with open('src/ui/MainWindow.h', 'w') as f:
        f.write(h_content)

with open('src/ui/MainWindow.cpp', 'r') as f:
    content = f.read()

# Initialization
if 'm_unprocessAction(nullptr)' not in content:
    content = content.replace('m_reprocessAction(nullptr),', 'm_reprocessAction(nullptr), m_unprocessAction(nullptr),')

# Creation
setup_actions_target = 'm_reprocessAction = new QAction(tr("&Reprocess"), this);'
if 'm_unprocessAction = ' not in content:
    unprocess_init = '''
  m_unprocessAction = new QAction(tr("&Send back to Inbox"), this);
  connect(m_unprocessAction, &QAction::triggered, this,
          [this]() { onItemAction(ItemState::Unprocessed); });
  actionCollection()->addAction("unprocess_item", m_unprocessAction);
'''
    content = content.replace(setup_actions_target, setup_actions_target + unprocess_init)

# Menu Addition
menu_target = '''
  } else {
    menu.addAction(m_queueAction);
    menu.addAction(m_holdAction);
  }
'''
menu_replace = '''
  } else {
    menu.addAction(m_queueAction);
    menu.addAction(m_holdAction);
  }
  if (view != m_unprocessedView) {
    menu.addSeparator();
    menu.addAction(m_unprocessAction);
  }
'''
if 'menu.addAction(m_unprocessAction);' not in content:
    content = content.replace(menu_target, menu_replace)
    # Also add to error view
    error_view_target = '''
  } else if (view == m_errorView) {
    menu.addAction(m_reprocessAction);
    menu.addAction(m_dismissAction);
    menu.addSeparator();
    menu.addAction(m_queueAction);
    menu.addAction(m_holdAction);
  }'''
    error_view_replace = '''
  } else if (view == m_errorView) {
    menu.addAction(m_reprocessAction);
    menu.addAction(m_dismissAction);
    menu.addSeparator();
    menu.addAction(m_queueAction);
    menu.addAction(m_holdAction);
  }'''
    content = content.replace(error_view_target, error_view_replace) # It's already handled by view != m_unprocessedView if block above which we placed correctly

# Wait, let's just insert it cleanly in onCustomContextMenuRequested
menu_func_target = '''
  } else if (view == m_errorView) {
    menu.addAction(m_reprocessAction);
    menu.addAction(m_dismissAction);
    menu.addSeparator();
    menu.addAction(m_queueAction);
    menu.addAction(m_holdAction);
  } else {
    menu.addAction(m_queueAction);
    menu.addAction(m_holdAction);
  }

  menu.addAction(m_archiveAction);
'''

menu_func_replace = '''
  } else if (view == m_errorView) {
    menu.addAction(m_reprocessAction);
    menu.addAction(m_dismissAction);
    menu.addSeparator();
    menu.addAction(m_queueAction);
    menu.addAction(m_holdAction);
    menu.addAction(m_unprocessAction);
  } else {
    menu.addAction(m_queueAction);
    menu.addAction(m_holdAction);
    menu.addAction(m_unprocessAction);
  }

  menu.addAction(m_archiveAction);
'''
if 'menu.addAction(m_unprocessAction);' not in content:
    content = content.replace(menu_func_target, menu_func_replace)


# Update Actions State
update_actions_target = '''
  if (m_reprocessAction) {
    m_reprocessAction->setVisible(view == m_errorView);
    m_reprocessAction->setEnabled(hasSelection && view == m_errorView);
  }
'''
update_actions_replace = '''
  if (m_reprocessAction) {
    m_reprocessAction->setVisible(view == m_errorView);
    m_reprocessAction->setEnabled(hasSelection && view == m_errorView);
  }

  if (m_unprocessAction) {
    m_unprocessAction->setVisible(view != m_unprocessedView);
    m_unprocessAction->setEnabled(hasSelection && view != m_unprocessedView);
  }
'''
if 'm_unprocessAction->setEnabled' not in content:
    content = content.replace(update_actions_target, update_actions_replace)

if 'm_unprocessAction->setEnabled(false);' not in content:
    content = content.replace('if (m_reprocessAction) m_reprocessAction->setEnabled(false);',
                              'if (m_reprocessAction) m_reprocessAction->setEnabled(false);\n    if (m_unprocessAction) m_unprocessAction->setEnabled(false);')

with open('src/ui/MainWindow.cpp', 'w') as f:
    f.write(content)
