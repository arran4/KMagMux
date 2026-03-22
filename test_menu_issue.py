# The user says "None of the right click options in the "inbox" tab meaning I can't test this."
# This means: in MainWindow, inside the "Inbox" tab, when they right click, they expect to see some options but there are "none".
# Wait. Look at MainWindow::onCustomContextMenuRequested.
'''
  // If we clicked on an empty space in the Inbox view, offer an "Add..." action
  if (!index.isValid()) {
    if (view == m_unprocessedView) {
      QAction *addAction = menu.addAction("Add...");
      connect(addAction, &QAction::triggered, this, &MainWindow::onAddItems);
      menu.exec(view->viewport()->mapToGlobal(pos));
    }
    return;
  }
'''
# If they right click an ITEM (index.isValid() is true), what happens?
'''
  // If we are in the Inbox view, offer a "Process..." action
  if (view == m_unprocessedView) {
    menu.addAction(m_processAction);
    menu.addSeparator();
  } else if (view == m_errorView) {
...
  menu.addAction(m_archiveAction);
  if (view == m_doneView || view == m_errorView) {
    menu.addAction(m_archiveAllAction);
  }
  menu.addSeparator();
  menu.addAction(m_deleteAction);

  menu.exec(view->viewport()->mapToGlobal(pos));
'''
# Wait, are m_processAction, m_archiveAction, m_deleteAction shown? Yes.
# But are they ENABLED?
# Let's look at updateActionsState.
'''
  if (m_processAction) {
    m_processAction->setVisible(view == m_unprocessedView);
    m_processAction->setEnabled(hasSelection && view == m_unprocessedView);
  }
'''
# `updateActionsState` is called on selection changes.
# It sets enablement. So if an item is selected, it's enabled. If they right click a row, is it selected?
# If they right click without left clicking first, it might not select the row, but usually right clicking a row in Qt selects it (if `QAbstractItemView::SelectRows` is set, and it is).
#
# But wait...
# "None of the right click options in the "inbox" tab meaning I can't test this."
# Does he mean the NEW right click options?
# What are the new options I was supposed to add to inbox?
# My plan:
# "Add a bulk archive option to done, and error." -> done
# "In Queue we need an option to "unprocess" / "send back to inbox" (same with error, and archive/done ... actually everywhere.)" -> done
# "Delete deletes the torrent from the managed storage and the metadata." -> That's just the behavior.
# "It should store it's complete history which is visible in any pane with a right click." -> Oh! "visible in any pane with a right click".
# I missed adding a right click option to view history! I only added a tab to the "Get Info" dialog!
# Ah! "visible in any pane with a right click". I did not add an explicit "View History" or "Get Info" right click option to `MainWindow`!
