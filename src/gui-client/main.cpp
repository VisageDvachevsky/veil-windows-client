#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QTranslator>
#include <QLocale>
#include <QSettings>
#include <QLibraryInfo>

#ifdef QT_NETWORK_LIB
#include <QSslSocket>
#endif

#include "mainwindow.h"

#ifdef _WIN32
#include "windows/service_manager.h"
#endif

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  // Log Qt and SSL information for debugging
  qDebug() << "=== VEIL VPN Client Startup ===";
  qDebug() << "Qt Version:" << qVersion();
  qDebug() << "Application Version: 0.1.0";

#ifdef QT_NETWORK_LIB
  // Check and log SSL/TLS backend support
  qDebug() << "Qt Network SSL Support:" << QSslSocket::supportsSsl();
  if (QSslSocket::supportsSsl()) {
    qDebug() << "SSL Library Build Version:" << QSslSocket::sslLibraryBuildVersionString();
    qDebug() << "SSL Library Runtime Version:" << QSslSocket::sslLibraryVersionString();
  } else {
    qWarning() << "WARNING: Qt Network does not support SSL/TLS!";
    qWarning() << "This may cause issues with HTTPS connections for update checks.";
    qWarning() << "The VPN tunnel itself is not affected (uses VEIL protocol).";
  }

  // List available TLS backends
  auto backends = QSslSocket::availableBackends();
  qDebug() << "Available TLS backends:" << backends;
  if (backends.isEmpty()) {
    qWarning() << "WARNING: No TLS backends available!";
    qWarning() << "Expected backends: 'schannel' (Windows native) or 'openssl'";
  }

  // Get active TLS backend
  QString activeBackend = QSslSocket::activeBackend();
  qDebug() << "Active TLS backend:" << (activeBackend.isEmpty() ? "none" : activeBackend);
#endif

  qDebug() << "===============================";

  // Set application metadata
  app.setOrganizationName("VEIL");
  app.setOrganizationDomain("veil.local");
  app.setApplicationName("VEIL Client");
  app.setApplicationVersion("0.1.0");

  // Load translations
  QSettings settings("VEIL", "VPN Client");
  QString languageCode = settings.value("ui/language", "en").toString();

  // Auto-detect system language if not set or invalid
  QStringList supportedLanguages = {"en", "ru", "zh"};
  if (!supportedLanguages.contains(languageCode)) {
    // Try to use system locale
    QString systemLocale = QLocale::system().name();  // e.g., "en_US", "ru_RU", "zh_CN"
    QString systemLang = systemLocale.left(2);  // Get language code (first 2 chars)

    if (supportedLanguages.contains(systemLang)) {
      languageCode = systemLang;
      qDebug() << "Auto-detected system language:" << systemLang;
    } else {
      languageCode = "en";  // Default to English
      qDebug() << "System language not supported, defaulting to English";
    }
  }

  qDebug() << "Loading translations for language:" << languageCode;

  // Load Qt's built-in translations (for standard dialogs)
  QTranslator qtTranslator;
  if (qtTranslator.load("qt_" + languageCode, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
    app.installTranslator(&qtTranslator);
    qDebug() << "Loaded Qt base translations for" << languageCode;
  } else {
    qDebug() << "Failed to load Qt base translations for" << languageCode;
  }

  // Load application translations
  QTranslator appTranslator;
  QString translationsPath = QCoreApplication::applicationDirPath() + "/translations";
  QString translationFile = "veil_" + languageCode;

  qDebug() << "Looking for translation file:" << translationFile << "in" << translationsPath;

  if (appTranslator.load(translationFile, translationsPath)) {
    app.installTranslator(&appTranslator);
    qDebug() << "Successfully loaded application translations:" << translationFile;
  } else {
    // Try to load from resource path (for bundled translations)
    if (appTranslator.load(":/translations/" + translationFile)) {
      app.installTranslator(&appTranslator);
      qDebug() << "Successfully loaded application translations from resources:" << translationFile;
    } else {
      qDebug() << "Warning: Failed to load application translations for" << languageCode;
      qDebug() << "Tried paths:" << translationsPath << "and :/translations/";
    }
  }

#ifdef _WIN32
  // On Windows, check if we have admin rights. If not, request elevation.
  // Admin rights are needed to start/manage the VPN service.
  if (!veil::windows::elevation::is_elevated()) {
    // Not elevated - request elevation and restart
    QMessageBox::information(
        nullptr,
        QObject::tr("Administrator Rights Required"),
        QObject::tr("VEIL VPN Client requires administrator privileges\n"
                    "to manage the VPN service.\n\n"
                    "The application will now request elevation."));

    // Request elevation - this will restart the app as admin
    if (veil::windows::elevation::request_elevation("")) {
      // Elevated process was started, exit this instance
      return 0;
    }

    // User declined or elevation failed
    QMessageBox::critical(
        nullptr,
        QObject::tr("Elevation Failed"),
        QObject::tr("Administrator privileges are required to run VEIL VPN.\n\n"
                    "Please run the application as Administrator."));
    return 1;
  }
#endif

  // Check for command-line arguments
  QStringList args = app.arguments();
  bool startMinimized = args.contains("--minimized") || args.contains("-m");

  // Create main window
  veil::gui::MainWindow window;

  // Show window unless minimized flag is set
  if (!startMinimized) {
    window.show();
  } else {
    qDebug() << "Starting minimized due to --minimized flag";
    // Window will be hidden by the startMinimized logic in MainWindow constructor
    window.show();  // Still call show() first, then hide() in constructor
  }

  return app.exec();
}
