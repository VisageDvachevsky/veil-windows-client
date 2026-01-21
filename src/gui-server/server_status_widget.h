#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QElapsedTimer>
#include <QGraphicsOpacityEffect>

namespace veil::gui {

enum class ServerState {
  kStopped,
  kStarting,
  kRunning,
  kStopping
};

class ServerStatusWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ServerStatusWidget(QWidget* parent = nullptr);

 signals:
  void startRequested();
  void stopRequested();

 public slots:
  void setServerState(ServerState state);
  void updateMetrics(uint64_t bytesSent, uint64_t bytesReceived,
                     int activeClients, int maxClients);
  void setListenAddress(const QString& address, int port);

 private slots:
  void onStartStopClicked();
  void updateUptime();
  void updatePulseAnimation();
  void simulateDemoData();

 private:
  void setupUi();
  void updateStatusIndicator();
  QString formatBytes(uint64_t bytes) const;
  QString formatUptime(qint64 seconds) const;

  // State
  ServerState state_ = ServerState::kStopped;

  // UI Elements
  QWidget* statusIndicator_;
  QLabel* statusLabel_;
  QLabel* listenAddressLabel_;
  QLabel* activeClientsLabel_;
  QLabel* maxClientsLabel_;
  QLabel* uptimeLabel_;
  QLabel* bytesSentLabel_;
  QLabel* bytesReceivedLabel_;
  QPushButton* startStopButton_;

  // Animation
  QTimer* pulseTimer_;
  QTimer* uptimeTimer_;
  QTimer* demoTimer_;
  QGraphicsOpacityEffect* indicatorOpacity_;
  QElapsedTimer uptimeCounter_;
  float pulsePhase_ = 0.0f;

  // Demo data
  uint64_t demoBytesSent_ = 0;
  uint64_t demoBytesReceived_ = 0;
  int demoClients_ = 0;
};

}  // namespace veil::gui
