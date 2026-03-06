import os

filepath = 'src/ui/PreferencesDialog.h'
with open(filepath, 'r') as f:
    content = f.read()

new_content = content.replace(
    'class PreferencesDialog : public QDialog {',
    '''class Engine;

class PreferencesDialog : public QDialog {'''
)

new_content = new_content.replace(
    '  explicit PreferencesDialog(QWidget *parent = nullptr);',
    '  explicit PreferencesDialog(Engine *engine, QWidget *parent = nullptr);'
)

new_content = new_content.replace(
    '  void createShortcutsPage();',
    '  void createShortcutsPage();\n  void createPluginsPage();\n\n  Engine *m_engine;'
)

with open(filepath, 'w') as f:
    f.write(new_content)

filepath = 'src/ui/PreferencesDialog.cpp'
with open(filepath, 'r') as f:
    content = f.read()

new_content = content.replace(
    '#include <QVBoxLayout>',
    '#include <QVBoxLayout>\n#include "../core/Engine.h"\n#include "../core/Connector.h"\n#include <QScrollArea>\n#include <QGroupBox>'
)

new_content = new_content.replace(
    'PreferencesDialog::PreferencesDialog(QWidget *parent) : QDialog(parent) {',
    'PreferencesDialog::PreferencesDialog(Engine *engine, QWidget *parent) : QDialog(parent), m_engine(engine) {'
)

new_content = new_content.replace(
    '  createShortcutsPage();',
    '  createShortcutsPage();\n  createPluginsPage();'
)

with open(filepath, 'w') as f:
    f.write(new_content)

print("Patched PreferencesDialog")
