import os

filepath = 'src/ui/PreferencesDialog.cpp'
with open(filepath, 'r') as f:
    content = f.read()

new_content = content.replace(
    'resize(600, 400);',
    'resize(800, 600);'
)

new_content = new_content.replace(
    'm_categoriesList->setMaximumWidth(120);',
    'm_categoriesList->setMaximumWidth(160);'
)

with open(filepath, 'w') as f:
    f.write(new_content)

print("Patched PreferencesDialog to be larger.")
