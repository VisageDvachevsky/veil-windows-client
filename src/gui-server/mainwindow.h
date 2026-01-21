#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QTimer>

namespace veil::gui {

class ServerStatusWidget;
class ClientListWidget;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 private slots:
  void showAboutDialog();

 private:
  void setupUi();
  void setupMenuBar();
  void setupStatusBar();
  void applyDarkTheme();

  ServerStatusWidget* statusWidget_;
  ClientListWidget* clientListWidget_;
};

}  // namespace veil::gui
