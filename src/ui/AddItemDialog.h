#ifndef ADDITEMDIALOG_H
#define ADDITEMDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QDateTimeEdit>
#include "../core/Item.h"

class AddItemDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddItemDialog(Item &item, QWidget *parent = nullptr);

    Item getItem() const;
    bool shouldDeleteOriginal() const;

private slots:
    void onDispatchClicked();
    void onQueueClicked();
    void onScheduleClicked();
    void onHoldClicked();

private:
    Item m_item;

    QLineEdit *m_sourceEdit;
    QLineEdit *m_destEdit;
    QComboBox *m_connectorCombo;
    QLineEdit *m_labelsEdit;
    QCheckBox *m_deleteOriginalCheck;
    QDateTimeEdit *m_scheduleEdit;

    void setupUi();
};

#endif // ADDITEMDIALOG_H
