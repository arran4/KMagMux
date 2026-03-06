import os

filepath = 'src/ui/MainWindow.cpp'
with open(filepath, 'r') as f:
    content = f.read()

new_content = content.replace(
    'PreferencesDialog dialog(this);',
    'PreferencesDialog dialog(m_engine, this);'
)

with open(filepath, 'w') as f:
    f.write(new_content)

print("Patched MainWindow")
