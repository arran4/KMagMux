#ifndef APIEXPLORERDIALOG_H
#define APIEXPLORERDIALOG_H

#include "../core/Connector.h"
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPlainTextEdit>
#include <QTableWidget>

class ApiExplorerDialog : public QDialog {
  Q_OBJECT

public:
  explicit ApiExplorerDialog(Connector *connector, QWidget *parent = nullptr);
  ~ApiExplorerDialog();

private slots:
  void onEndpointSelected(int currentRow);
  void onAddHeader();
  void onRemoveHeader();
  void onSendRequest();
  void onNetworkReplyFinished();
  void onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);

private:
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
  QPlainTextEdit *m_bodyEdit;
  QPlainTextEdit *m_responseEdit;
  QLabel *m_statusLabel;
};

#endif // APIEXPLORERDIALOG_H
