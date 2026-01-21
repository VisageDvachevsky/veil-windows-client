#include "mainwindow.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QDialog>

#include "common/gui/theme.h"
#include "client_list_widget.h"
#include "server_status_widget.h"

namespace veil::gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      statusWidget_(new ServerStatusWidget(this)),
      clientListWidget_(new ClientListWidget(this)) {
  setupUi();
  setupMenuBar();
  setupStatusBar();
  applyDarkTheme();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
  setWindowTitle("VEIL VPN Server");
  setMinimumSize(800, 600);
  resize(1024, 768);

  // Set window flags for modern appearance
  setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);

  auto* centralWidget = new QWidget(this);
  auto* layout = new QVBoxLayout(centralWidget);
  layout->setSpacing(16);
  layout->setContentsMargins(24, 24, 24, 24);

  // Header
  auto* headerLayout = new QHBoxLayout();
  auto* titleLabel = new QLabel("VEIL Server", centralWidget);
  titleLabel->setStyleSheet(QString("font-size: %1px; font-weight: 700; color: %2;")
                                .arg(fonts::kFontSizeHeadline)
                                .arg(colors::dark::kAccentPrimary));
  headerLayout->addWidget(titleLabel);
  headerLayout->addStretch();
  layout->addLayout(headerLayout);

  // Status widget
  layout->addWidget(statusWidget_);

  // Client list widget (takes remaining space)
  layout->addWidget(clientListWidget_, 1);

  setCentralWidget(centralWidget);
}

void MainWindow::setupMenuBar() {
  menuBar()->setStyleSheet(R"(
    QMenuBar {
      background-color: #1a1d23;
      color: #eceff4;
      border-bottom: 1px solid rgba(255, 255, 255, 0.1);
      padding: 4px 8px;
    }
    QMenuBar::item {
      padding: 6px 12px;
      border-radius: 4px;
      margin: 2px;
    }
    QMenuBar::item:selected {
      background-color: #252932;
    }
    QMenu {
      background-color: #252932;
      border: 1px solid rgba(255, 255, 255, 0.1);
      border-radius: 8px;
      padding: 4px;
    }
    QMenu::item {
      padding: 8px 24px;
      border-radius: 4px;
    }
    QMenu::item:selected {
      background-color: #3aafff;
      color: white;
    }
  )");

  auto* serverMenu = menuBar()->addMenu(tr("&Server"));
  serverMenu->addAction(tr("&Start"), []() {
    // TODO: Implement server start
  });
  serverMenu->addAction(tr("S&top"), []() {
    // TODO: Implement server stop
  });
  serverMenu->addSeparator();
  serverMenu->addAction(tr("&Settings"), []() {
    // TODO: Show settings dialog
  });

  auto* clientsMenu = menuBar()->addMenu(tr("&Clients"));
  clientsMenu->addAction(tr("&Disconnect All"), []() {
    // TODO: Disconnect all clients
  });
  clientsMenu->addAction(tr("&Export Client List"), []() {
    // TODO: Export client list
  });

  auto* helpMenu = menuBar()->addMenu(tr("&Help"));
  helpMenu->addAction(tr("&About VEIL Server"), this, &MainWindow::showAboutDialog);
}

void MainWindow::setupStatusBar() {
  statusBar()->setStyleSheet(R"(
    QStatusBar {
      background-color: #1a1d23;
      color: #8fa1b3;
      border-top: 1px solid rgba(255, 255, 255, 0.1);
      padding: 4px 8px;
      font-size: 12px;
    }
    QStatusBar::item {
      border: none;
    }
  )");

  statusBar()->showMessage(tr("Server ready"));
}

void MainWindow::applyDarkTheme() {
  setStyleSheet(getDarkThemeStylesheet());

  // Additional window-specific styles
  QString windowStyle = R"(
    QMainWindow {
      background-color: #1a1d23;
    }
  )";

  setStyleSheet(styleSheet() + windowStyle);
}

void MainWindow::showAboutDialog() {
  auto* dialog = new QDialog(this);
  dialog->setWindowTitle(tr("About VEIL Server"));
  dialog->setModal(true);
  dialog->setFixedSize(400, 300);

  dialog->setStyleSheet(R"(
    QDialog {
      background-color: #1a1d23;
      color: #eceff4;
    }
    QLabel {
      color: #eceff4;
    }
    QPushButton {
      background: #3aafff;
      border: none;
      border-radius: 8px;
      padding: 10px 24px;
      color: white;
      font-weight: 600;
    }
    QPushButton:hover {
      background: #4abfff;
    }
  )");

  auto* layout = new QVBoxLayout(dialog);
  layout->setSpacing(16);
  layout->setContentsMargins(32, 32, 32, 32);

  auto* titleLabel = new QLabel("VEIL VPN Server", dialog);
  titleLabel->setStyleSheet("font-size: 24px; font-weight: 700; color: #3aafff;");
  titleLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(titleLabel);

  auto* versionLabel = new QLabel("Version 0.1.0", dialog);
  versionLabel->setStyleSheet("color: #8fa1b3; font-size: 14px;");
  versionLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(versionLabel);

  auto* descLabel = new QLabel(
      "A secure UDP-based VPN server with DPI evasion capabilities.\n\n"
      "Supports multiple clients with session management,\n"
      "traffic monitoring, and real-time statistics.",
      dialog);
  descLabel->setWordWrap(true);
  descLabel->setStyleSheet("color: #8fa1b3; font-size: 13px;");
  descLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(descLabel);

  layout->addStretch();

  auto* closeButton = new QPushButton(tr("Close"), dialog);
  connect(closeButton, &QPushButton::clicked, dialog, &QDialog::accept);
  layout->addWidget(closeButton, 0, Qt::AlignCenter);

  dialog->exec();
  dialog->deleteLater();
}

}  // namespace veil::gui
