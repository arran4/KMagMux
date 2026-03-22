import sys

with open('src/ui/PreferencesDialog.h', 'r') as f:
    h_content = f.read()

if 'QSpinBox *m_autoArchiveDays;' not in h_content:
    h_content = h_content.replace('#include <QDialog>', '#include <QDialog>\n#include <QSpinBox>')
    h_content = h_content.replace('QComboBox *m_autoMoveInboxCombo;', 'QComboBox *m_autoMoveInboxCombo;\n  QSpinBox *m_autoArchiveDays;')
    with open('src/ui/PreferencesDialog.h', 'w') as f:
        f.write(h_content)

with open('src/ui/PreferencesDialog.cpp', 'r') as f:
    content = f.read()

# Add to createGeneralPage
target_page = 'm_autoStartCb->setChecked(settings.value("autoStart", false).toBool());'
replacement_page = '''m_autoStartCb->setChecked(settings.value("autoStart", false).toBool());

  QHBoxLayout *autoArchiveLayout = new QHBoxLayout();
  QLabel *autoArchiveLabel = new QLabel(tr("Auto Archive 'Done' items after (days):"), page);
  m_autoArchiveDays = new QSpinBox(page);
  m_autoArchiveDays->setRange(0, 3650);
  m_autoArchiveDays->setSpecialValueText(tr("Disabled"));
  m_autoArchiveDays->setValue(settings.value("autoArchiveDays", 0).toInt());
  autoArchiveLayout->addWidget(autoArchiveLabel);
  autoArchiveLayout->addWidget(m_autoArchiveDays);
  autoArchiveLayout->addStretch();
  layout->addLayout(autoArchiveLayout);'''
if 'm_autoArchiveDays = new QSpinBox' not in content:
    content = content.replace(target_page, replacement_page)

# Add to apply and accept lambdas
target_save = 'settings.setValue("autoStart", m_autoStartCb->isChecked());'
replacement_save = 'settings.setValue("autoStart", m_autoStartCb->isChecked());\n            settings.setValue("autoArchiveDays", m_autoArchiveDays->value());'
content = content.replace(target_save, replacement_save)

with open('src/ui/PreferencesDialog.cpp', 'w') as f:
    f.write(content)

with open('src/core/Engine.cpp', 'r') as f:
    e_content = f.read()

if '#include <QSettings>' not in e_content:
    e_content = e_content.replace('#include "Engine.h"', '#include "Engine.h"\n#include <QSettings>')

target_engine = '''
  auto items =
      m_storage->loadItemsByStates({ItemState::Queued, ItemState::Scheduled});
'''
replacement_engine = '''
  auto items =
      m_storage->loadItemsByStates({ItemState::Queued, ItemState::Scheduled});

  QSettings settings;
  int autoArchiveDays = settings.value("autoArchiveDays", 0).toInt();
  if (autoArchiveDays > 0) {
    auto doneItems = m_storage->loadItemsByStates({ItemState::Done});
    std::vector<Item> itemsToArchive;
    QDateTime threshold = QDateTime::currentDateTime().addDays(-autoArchiveDays);

    for (auto &item : doneItems) {
      if (!item.metadata["lastDispatchTime"].toString().isEmpty()) {
        QDateTime lastDispatch = QDateTime::fromString(item.metadata["lastDispatchTime"].toString(), Qt::ISODate);
        if (lastDispatch.isValid() && lastDispatch < threshold) {
          item.state = ItemState::Archived;
          item.addHistory(QString("Auto-archived after %1 days in Done state.").arg(autoArchiveDays));
          itemsToArchive.push_back(item);
        }
      }
    }
    if (!itemsToArchive.empty()) {
      m_storage->saveItems(itemsToArchive);
    }
  }
'''
if 'autoArchiveDays > 0' not in e_content:
    e_content = e_content.replace(target_engine, replacement_engine)
    with open('src/core/Engine.cpp', 'w') as f:
        f.write(e_content)
