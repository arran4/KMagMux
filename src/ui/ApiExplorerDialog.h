#ifndef APIEXPLORERDIALOG_H
#define APIEXPLORERDIALOG_H

#include <QByteArray>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPlainTextEdit>
#include <QSslError>
#include <QString>
#include <QTableWidget>
#include <QWidget>

class ApiExplorerDialog : public QDialog {
  Q_OBJECT

public:
  explicit ApiExplorerDialog(Connector *connector, QWidget *parent = nullptr);
  ~ApiExplorerDialog();

private Q_SLOTS:
  void onEndpointSelected(int currentRow);
  static void onAddHeader();
  static void onRemoveHeader();
  static void onAddMultipart();
  static void onRemoveMultipart();
  static void onBrowseMultipartFile();
  void onSendRequest() const;
  static void onNetworkReplyFinished();
  void onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);

  void setupUi();
  void loadEndpoints();
  void populateForm(const HttpApiEndpoint &endpoint);
  QString applySubstitutions(QString text) const;
  QByteArray applySubstitutions(QByteArray data) const;

  Connector *m_connector;
  QList<HttpApiEndpoint> m_endpoints;
  QMap<QString, QString> m_substitutions;
  QNetworkAccessManager *m_networkManager;
  QNetworkReply *m_currentReply;

  QListWidget *m_endpointList;
  QComboBox *m_methodCombo;
  QLineEdit *m_urlEdit;
  QTableWidget *m_headersTable;
  QTableWidget *m_multipartTable;
  QWidget *m_multipartWidget;
  QWidget *m_rawBodyWidget;
  QPlainTextEdit *m_bodyEdit;
  QPlainTextEdit *m_responseEdit;
  QLabel *m_statusLabel;
  bool m_isMultipartCurrent;
};

#endif // APIEXPLORERDIALOG_H
