import os

filepath = 'src/core/Connector.h'
with open(filepath, 'r') as f:
    content = f.read()

new_content = content.replace(
    '  virtual void dispatch(const Item &item) = 0;\n};',
    '''  virtual void dispatch(const Item &item) = 0;

  virtual bool hasSettings() const { return false; }
  virtual class QWidget* createSettingsWidget(class QWidget* parent) { return nullptr; }
  virtual void saveSettings(class QWidget* settingsWidget) {}
};'''
)

with open(filepath, 'w') as f:
    f.write(new_content)

print("Patched Connector.h")
