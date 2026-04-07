#ifndef LINKEXTRACTORDIALOG_H
#define LINKEXTRACTORDIALOG_H

#include <QCheckBox>
#include <QCloseEvent>
#include <QDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QWidget>

class LinkExtractorDialog : public QDialog {
  Q_OBJECT
public:
  explicit LinkExtractorDialog(const QStringList &lines, bool extractMagnets,
                               bool extractTorrents, QWidget *parent = nullptr);
  QStringList getExpandedLines() const { return m_expandedLines; }
  bool wasModified() const { return m_modified; }

protected:
  void closeEvent(QCloseEvent *event) override;

private Q_SLOTS:
  void processNext();
  void onReplyFinished();
  void onCancelClicked();

  void appendLog(const QString &msg);
  void extractFromHtml(const QString &content, const QUrl &baseUrl);
  void extractFromTxt(const QString &content);

  QStringList m_inputLines;
  QStringList m_expandedLines;
  int m_currentIndex;
  bool m_cancelled;
  bool m_modified;

  QNetworkAccessManager *m_networkManager;
  QNetworkReply *m_currentReply;

  QPlainTextEdit *m_logEdit;
  QPushButton *m_cancelBtn;
  QCheckBox *m_extractMagnetsCb;
  QCheckBox *m_extractTorrentsCb;
};

#endif // LINKEXTRACTORDIALOG_H
