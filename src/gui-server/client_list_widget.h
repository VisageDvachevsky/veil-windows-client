#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTimer>
#include <QMenu>

namespace veil::gui {

struct ClientInfo {
  QString sessionId;
  QString tunnelIp;
  QString endpoint;
  qint64 connectedAt;  // timestamp in seconds
  uint64_t bytesSent;
  uint64_t bytesReceived;
  int latencyMs;
  QString dpiMode;
};

class ClientListWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ClientListWidget(QWidget* parent = nullptr);

 signals:
  void clientDisconnectRequested(const QString& sessionId);
  void clientDetailsRequested(const QString& sessionId);

 public slots:
  void addClient(const ClientInfo& client);
  void removeClient(const QString& sessionId);
  void updateClient(const ClientInfo& client);
  void clearAllClients();

 private slots:
  void onSearchTextChanged(const QString& text);
  void onTableContextMenu(const QPoint& pos);
  void onDisconnectClient();
  void onViewClientDetails();
  void updateDemoData();

 private:
  void setupUi();
  void updateClientRow(int row, const ClientInfo& client);
  int findClientRow(const QString& sessionId) const;
  QString formatBytes(uint64_t bytes) const;
  QString formatUptime(qint64 connectedAt) const;
  QString formatLatency(int latencyMs) const;

  // UI Elements
  QLabel* clientCountLabel_;
  QLineEdit* searchEdit_;
  QTableWidget* tableWidget_;
  QPushButton* disconnectAllButton_;
  QPushButton* refreshButton_;

  // Context menu
  QMenu* contextMenu_;
  QAction* disconnectAction_;
  QAction* viewDetailsAction_;

  // Demo timer
  QTimer* demoTimer_;
  QVector<ClientInfo> demoClients_;
};

}  // namespace veil::gui
