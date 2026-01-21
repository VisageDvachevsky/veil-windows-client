#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QDialog>
#include <QLabel>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QCloseEvent>
#include <memory>

namespace veil::gui {

class ConnectionWidget;
class SettingsWidget;
class DiagnosticsWidget;
class IpcClientManager;

/// Connection state for system tray icon updates
enum class TrayConnectionState {
  kDisconnected,
  kConnecting,
  kConnected,
  kError
};

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 public slots:
  /// Update the system tray icon based on connection state
  void updateTrayIcon(TrayConnectionState state);

 protected:
  /// Handle window close event - minimize to tray if enabled
  void closeEvent(QCloseEvent* event) override;

 private slots:
  void showConnectionView();
  void showSettingsView();
  void showDiagnosticsView();
  void showAboutDialog();
  void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
  void onQuickConnect();
  void onQuickDisconnect();

 private:
  void setupUi();
  void setupMenuBar();
  void setupStatusBar();
  void setupSystemTray();
  void applyDarkTheme();

  QStackedWidget* stackedWidget_;
  ConnectionWidget* connectionWidget_;
  SettingsWidget* settingsWidget_;
  DiagnosticsWidget* diagnosticsWidget_;
  std::unique_ptr<IpcClientManager> ipcManager_;

  // System tray
  QSystemTrayIcon* trayIcon_;
  QMenu* trayMenu_;
  QAction* trayConnectAction_;
  QAction* trayDisconnectAction_;
  bool minimizeToTray_{true};
  TrayConnectionState currentTrayState_{TrayConnectionState::kDisconnected};
};

}  // namespace veil::gui
