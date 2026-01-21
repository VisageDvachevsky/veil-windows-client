#include "settings_widget.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QRegularExpression>
#include <QMessageBox>
#include <QTimer>

#include "common/gui/theme.h"

namespace veil::gui {

SettingsWidget::SettingsWidget(QWidget* parent) : QWidget(parent) {
  setupUi();
  loadSettings();
}

void SettingsWidget::setupUi() {
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(20);
  mainLayout->setContentsMargins(spacing::kPaddingXLarge, spacing::kPaddingMedium,
                                  spacing::kPaddingXLarge, spacing::kPaddingMedium);

  // === Header ===
  auto* headerLayout = new QHBoxLayout();

  auto* backButton = new QPushButton("\u2190 Back", this);
  backButton->setCursor(Qt::PointingHandCursor);
  backButton->setStyleSheet(R"(
    QPushButton {
      background: transparent;
      border: none;
      color: #58a6ff;
      font-size: 14px;
      font-weight: 500;
      padding: 8px 0;
      text-align: left;
    }
    QPushButton:hover {
      color: #79c0ff;
    }
  )");
  connect(backButton, &QPushButton::clicked, this, &SettingsWidget::backRequested);
  headerLayout->addWidget(backButton);

  headerLayout->addStretch();
  mainLayout->addLayout(headerLayout);

  // Title
  auto* titleLabel = new QLabel("Settings", this);
  titleLabel->setStyleSheet(QString("font-size: %1px; font-weight: 700; color: #f0f6fc; margin-bottom: 8px;")
                                .arg(fonts::kFontSizeHeadline));
  mainLayout->addWidget(titleLabel);

  // === Scrollable content ===
  auto* scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");

  auto* scrollWidget = new QWidget();
  scrollWidget->setStyleSheet("background: transparent;");
  auto* scrollLayout = new QVBoxLayout(scrollWidget);
  scrollLayout->setSpacing(16);
  scrollLayout->setContentsMargins(0, 0, 12, 0);  // Right margin for scrollbar

  // Create sections
  createServerSection(scrollWidget);
  createRoutingSection(scrollWidget);
  createConnectionSection(scrollWidget);
  createDpiBypassSection(scrollWidget);
  createAdvancedSection(scrollWidget);

  scrollLayout->addStretch();
  scrollArea->setWidget(scrollWidget);
  mainLayout->addWidget(scrollArea, 1);  // Stretch factor 1 to fill available space

  // === Footer buttons ===
  auto* footerLayout = new QHBoxLayout();
  footerLayout->setSpacing(12);

  resetButton_ = new QPushButton("Reset to Defaults", this);
  resetButton_->setCursor(Qt::PointingHandCursor);
  resetButton_->setStyleSheet(R"(
    QPushButton {
      background: transparent;
      border: 1px solid rgba(255, 255, 255, 0.15);
      border-radius: 12px;
      color: #8b949e;
      padding: 14px 24px;
      font-weight: 500;
    }
    QPushButton:hover {
      background: rgba(255, 255, 255, 0.04);
      border-color: rgba(255, 255, 255, 0.2);
      color: #f0f6fc;
    }
  )");
  connect(resetButton_, &QPushButton::clicked, this, &SettingsWidget::loadSettings);
  footerLayout->addWidget(resetButton_);

  footerLayout->addStretch();

  saveButton_ = new QPushButton("Save Changes", this);
  saveButton_->setCursor(Qt::PointingHandCursor);
  saveButton_->setStyleSheet(R"(
    QPushButton {
      background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                  stop:0 #238636, stop:1 #2ea043);
      border: none;
      border-radius: 12px;
      padding: 14px 28px;
      color: white;
      font-size: 15px;
      font-weight: 600;
    }
    QPushButton:hover {
      background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                  stop:0 #2ea043, stop:1 #3fb950);
    }
  )");
  connect(saveButton_, &QPushButton::clicked, this, &SettingsWidget::saveSettings);
  footerLayout->addWidget(saveButton_);

  mainLayout->addLayout(footerLayout);
}

void SettingsWidget::createServerSection(QWidget* parent) {
  auto* group = new QGroupBox("Server Configuration", parent);
  auto* layout = new QVBoxLayout(group);
  layout->setSpacing(12);

  // Server Address
  auto* serverLabel = new QLabel("Server Address", group);
  serverLabel->setProperty("textStyle", "secondary");
  layout->addWidget(serverLabel);

  serverAddressEdit_ = new QLineEdit(group);
  serverAddressEdit_->setPlaceholderText("vpn.example.com or 192.168.1.1");
  connect(serverAddressEdit_, &QLineEdit::textChanged, this, &SettingsWidget::onServerAddressChanged);
  layout->addWidget(serverAddressEdit_);

  serverValidationLabel_ = new QLabel(group);
  serverValidationLabel_->setStyleSheet(QString("color: %1; font-size: 12px;").arg(colors::dark::kAccentError));
  serverValidationLabel_->hide();
  layout->addWidget(serverValidationLabel_);

  // Port
  auto* portRow = new QHBoxLayout();
  auto* portLabel = new QLabel("Port", group);
  portLabel->setProperty("textStyle", "secondary");
  portRow->addWidget(portLabel);
  portRow->addStretch();

  portSpinBox_ = new QSpinBox(group);
  portSpinBox_->setRange(1, 65535);
  portSpinBox_->setValue(4433);
  portSpinBox_->setFixedWidth(100);
  connect(portSpinBox_, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsWidget::onPortChanged);
  portRow->addWidget(portSpinBox_);

  layout->addLayout(portRow);

  parent->layout()->addWidget(group);
}

void SettingsWidget::createRoutingSection(QWidget* parent) {
  auto* group = new QGroupBox("Routing", parent);
  auto* layout = new QVBoxLayout(group);
  layout->setSpacing(12);

  routeAllTrafficCheck_ = new QCheckBox("Route all traffic through VPN", group);
  routeAllTrafficCheck_->setToolTip("Send all internet traffic through the VPN tunnel");
  layout->addWidget(routeAllTrafficCheck_);

  splitTunnelCheck_ = new QCheckBox("Split tunnel mode", group);
  splitTunnelCheck_->setToolTip("Only route specific networks through VPN");
  layout->addWidget(splitTunnelCheck_);

  // Custom routes (only visible when split tunnel is enabled)
  auto* customRoutesLabel = new QLabel("Custom Routes (CIDR notation)", group);
  customRoutesLabel->setProperty("textStyle", "secondary");
  layout->addWidget(customRoutesLabel);

  customRoutesEdit_ = new QLineEdit(group);
  customRoutesEdit_->setPlaceholderText("10.0.0.0/8, 192.168.0.0/16");
  customRoutesEdit_->setEnabled(false);
  layout->addWidget(customRoutesEdit_);

  // Connect split tunnel checkbox to enable/disable custom routes
  connect(splitTunnelCheck_, &QCheckBox::toggled, customRoutesEdit_, &QLineEdit::setEnabled);
  connect(routeAllTrafficCheck_, &QCheckBox::toggled, [this](bool checked) {
    if (checked) {
      splitTunnelCheck_->setChecked(false);
    }
  });
  connect(splitTunnelCheck_, &QCheckBox::toggled, [this](bool checked) {
    if (checked) {
      routeAllTrafficCheck_->setChecked(false);
    }
  });

  parent->layout()->addWidget(group);
}

void SettingsWidget::createConnectionSection(QWidget* parent) {
  auto* group = new QGroupBox("Connection", parent);
  auto* layout = new QVBoxLayout(group);
  layout->setSpacing(12);

  autoReconnectCheck_ = new QCheckBox("Auto-reconnect on disconnect", group);
  autoReconnectCheck_->setToolTip("Automatically try to reconnect when connection is lost");
  layout->addWidget(autoReconnectCheck_);

  // Reconnect interval
  auto* intervalRow = new QHBoxLayout();
  auto* intervalLabel = new QLabel("Reconnect Interval", group);
  intervalLabel->setProperty("textStyle", "secondary");
  intervalRow->addWidget(intervalLabel);
  intervalRow->addStretch();

  reconnectIntervalSpinBox_ = new QSpinBox(group);
  reconnectIntervalSpinBox_->setRange(1, 60);
  reconnectIntervalSpinBox_->setValue(5);
  reconnectIntervalSpinBox_->setSuffix(" sec");
  reconnectIntervalSpinBox_->setFixedWidth(100);
  intervalRow->addWidget(reconnectIntervalSpinBox_);

  layout->addLayout(intervalRow);

  // Max reconnect attempts
  auto* attemptsRow = new QHBoxLayout();
  auto* attemptsLabel = new QLabel("Max Reconnect Attempts", group);
  attemptsLabel->setProperty("textStyle", "secondary");
  attemptsRow->addWidget(attemptsLabel);
  attemptsRow->addStretch();

  maxReconnectAttemptsSpinBox_ = new QSpinBox(group);
  maxReconnectAttemptsSpinBox_->setRange(0, 100);
  maxReconnectAttemptsSpinBox_->setValue(5);
  maxReconnectAttemptsSpinBox_->setSpecialValueText("Unlimited");
  maxReconnectAttemptsSpinBox_->setFixedWidth(100);
  attemptsRow->addWidget(maxReconnectAttemptsSpinBox_);

  layout->addLayout(attemptsRow);

  parent->layout()->addWidget(group);
}

void SettingsWidget::createDpiBypassSection(QWidget* parent) {
  auto* group = new QGroupBox("DPI Bypass Mode", parent);
  auto* layout = new QVBoxLayout(group);
  layout->setSpacing(12);

  auto* modeLabel = new QLabel("Select traffic obfuscation mode:", group);
  modeLabel->setProperty("textStyle", "secondary");
  layout->addWidget(modeLabel);

  dpiModeCombo_ = new QComboBox(group);
  dpiModeCombo_->addItem("IoT Mimic", "iot");
  dpiModeCombo_->addItem("QUIC-Like", "quic");
  dpiModeCombo_->addItem("Random-Noise Stealth", "random");
  dpiModeCombo_->addItem("Trickle Mode", "trickle");
  connect(dpiModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &SettingsWidget::onDpiModeChanged);
  layout->addWidget(dpiModeCombo_);

  dpiDescLabel_ = new QLabel(group);
  dpiDescLabel_->setWordWrap(true);
  dpiDescLabel_->setStyleSheet(QString("color: %1; font-size: 12px; padding: 12px; "
                                       "background: rgba(88, 166, 255, 0.08); "
                                       "border: 1px solid rgba(88, 166, 255, 0.2); "
                                       "border-radius: 10px;")
                                   .arg(colors::dark::kAccentPrimary));
  layout->addWidget(dpiDescLabel_);

  // Set initial description
  onDpiModeChanged(0);

  parent->layout()->addWidget(group);
}

void SettingsWidget::createAdvancedSection(QWidget* parent) {
  auto* group = new QGroupBox("Advanced", parent);
  auto* layout = new QVBoxLayout(group);
  layout->setSpacing(12);

  obfuscationCheck_ = new QCheckBox("Enable obfuscation", group);
  obfuscationCheck_->setToolTip("Enable traffic morphing with padding and timing jitter");
  layout->addWidget(obfuscationCheck_);

  verboseLoggingCheck_ = new QCheckBox("Verbose logging", group);
  verboseLoggingCheck_->setToolTip("Log detailed handshake and retransmission information");
  layout->addWidget(verboseLoggingCheck_);

  developerModeCheck_ = new QCheckBox("Developer mode", group);
  developerModeCheck_->setToolTip("Enable diagnostics screen with protocol metrics");
  layout->addWidget(developerModeCheck_);

  parent->layout()->addWidget(group);
}

void SettingsWidget::onServerAddressChanged() {
  validateSettings();
  hasUnsavedChanges_ = true;
}

void SettingsWidget::onPortChanged() {
  hasUnsavedChanges_ = true;
}

void SettingsWidget::onDpiModeChanged(int index) {
  static const char* descriptions[] = {
      "Simulates IoT sensor traffic with periodic heartbeats. "
      "Good balance of stealth and performance. Recommended for most users.",

      "Mimics modern HTTP/3 (QUIC) traffic patterns. "
      "Best for high-throughput scenarios where QUIC traffic is common.",

      "Maximum unpredictability with randomized packet sizes and timing. "
      "Use in extreme censorship environments. Higher overhead.",

      "Low-and-slow traffic with minimal bandwidth (10-50 kbit/s). "
      "Maximum stealth but not suitable for normal browsing."
  };

  if (index >= 0 && index < 4) {
    dpiDescLabel_->setText(descriptions[index]);
  }
  hasUnsavedChanges_ = true;
}

void SettingsWidget::validateSettings() {
  QString address = serverAddressEdit_->text().trimmed();
  bool isValid = address.isEmpty() || isValidHostname(address) || isValidIpAddress(address);

  if (!isValid && !address.isEmpty()) {
    serverValidationLabel_->setText("Invalid server address format");
    serverValidationLabel_->show();
    serverAddressEdit_->setStyleSheet(QString("border-color: %1;").arg(colors::dark::kAccentError));
  } else {
    serverValidationLabel_->hide();
    serverAddressEdit_->setStyleSheet("");
  }

  saveButton_->setEnabled(isValid || address.isEmpty());
}

bool SettingsWidget::isValidHostname(const QString& hostname) const {
  // Simple hostname validation
  static QRegularExpression hostnameRegex(
      "^([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)*[a-zA-Z]{2,}$");
  return hostnameRegex.match(hostname).hasMatch();
}

bool SettingsWidget::isValidIpAddress(const QString& ip) const {
  // IPv4 validation
  static QRegularExpression ipv4Regex(
      "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
  return ipv4Regex.match(ip).hasMatch();
}

void SettingsWidget::loadSettings() {
  // Load default settings (in real implementation, load from config file)
  serverAddressEdit_->setText("vpn.example.com");
  portSpinBox_->setValue(4433);
  routeAllTrafficCheck_->setChecked(true);
  splitTunnelCheck_->setChecked(false);
  customRoutesEdit_->clear();
  autoReconnectCheck_->setChecked(true);
  reconnectIntervalSpinBox_->setValue(5);
  maxReconnectAttemptsSpinBox_->setValue(5);
  dpiModeCombo_->setCurrentIndex(0);
  obfuscationCheck_->setChecked(true);
  verboseLoggingCheck_->setChecked(false);
  developerModeCheck_->setChecked(false);

  hasUnsavedChanges_ = false;
}

void SettingsWidget::saveSettings() {
  // Validate before saving
  validateSettings();

  if (!saveButton_->isEnabled()) {
    QMessageBox::warning(this, "Invalid Settings",
                         "Please fix the validation errors before saving.");
    return;
  }

  // In real implementation, save to config file
  // For now, just emit signal and show confirmation
  hasUnsavedChanges_ = false;

  // Show brief confirmation
  saveButton_->setText("Saved!");
  saveButton_->setStyleSheet(QString(R"(
    QPushButton {
      background: %1;
    }
  )").arg(colors::dark::kAccentSuccess));

  // Reset button after 2 seconds
  QTimer::singleShot(2000, this, [this]() {
    saveButton_->setText("Save Changes");
    saveButton_->setStyleSheet("");
  });

  emit settingsSaved();
}

}  // namespace veil::gui
