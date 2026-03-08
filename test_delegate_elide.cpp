#include <QApplication>
#include <QTableView>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QWidget>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QDebug>
#include <QTimer>

class MaxWidthDelegate : public QStyledItemDelegate {
public:
    MaxWidthDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QSize hint = QStyledItemDelegate::sizeHint(option, index);
        if (option.widget) {
            int max_w = option.widget->width() * 0.75;
            if (hint.width() > max_w) {
                return QSize(max_w, hint.height());
            }
        }
        return hint;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        // Let's force character level text elision
        opt.textElideMode = Qt::ElideRight;
        // Turn off word wrap just in case
        opt.features &= ~QStyleOptionViewItem::WrapText;

        QStyledItemDelegate::paint(painter, opt, index);
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget window;
    window.resize(600, 400);
    QVBoxLayout *layout = new QVBoxLayout(&window);
    QTableView *view = new QTableView();
    MaxWidthDelegate *delegate = new MaxWidthDelegate();
    view->setItemDelegate(delegate);
    QStandardItemModel *model = new QStandardItemModel(5, 5);
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            if (j == 2) {
                model->setItem(i, j, new QStandardItem("magnet:?xt=urn:btih:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855&dn=example_very_long_file_name_that_should_not_wrap_but_should_truncate_with_elipsis_character_by_character"));
            } else {
                model->setItem(i, j, new QStandardItem(QString("Item %1,%2").arg(i).arg(j)));
            }
        }
    }
    view->setModel(model);
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    view->horizontalHeader()->setStretchLastSection(true);
    view->setTextElideMode(Qt::ElideRight);
    // view->setWordWrap(false); // test with and without this
    layout->addWidget(view);
    window.show();

    QTimer::singleShot(1000, [&]() {
        app.quit();
    });

    return app.exec();
}
