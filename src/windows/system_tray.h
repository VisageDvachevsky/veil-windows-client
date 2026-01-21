#pragma once

#ifdef _WIN32

#include <windows.h>
#include <shellapi.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace veil::windows {

// ============================================================================
// System Tray Icon
// ============================================================================
// Provides system tray (notification area) integration for the VEIL VPN client.
// Allows quick access to connection status and common actions.

class SystemTray {
 public:
  // Connection states for icon display
  enum class ConnectionState {
    kDisconnected,
    kConnecting,
    kConnected,
    kError
  };

  // Menu item callback
  using MenuCallback = std::function<void()>;

  // Menu item definition
  struct MenuItem {
    std::string text;
    MenuCallback callback;
    bool enabled{true};
    bool checked{false};
    bool separator{false};  // If true, renders as separator
  };

  SystemTray();
  ~SystemTray();

  // Non-copyable
  SystemTray(const SystemTray&) = delete;
  SystemTray& operator=(const SystemTray&) = delete;

  // Initialize the system tray icon
  // window: Handle to the parent window for message processing
  // tooltip: Initial tooltip text
  bool init(HWND window, const std::string& tooltip);

  // Clean up the system tray icon
  void cleanup();

  // Update the icon based on connection state
  void set_state(ConnectionState state);

  // Update the tooltip text
  void set_tooltip(const std::string& tooltip);

  // Show a balloon notification
  // title: Notification title (max 64 characters)
  // message: Notification message (max 256 characters)
  // icon: NIIF_INFO, NIIF_WARNING, NIIF_ERROR, or NIIF_NONE
  void show_notification(const std::string& title, const std::string& message,
                         DWORD icon = NIIF_INFO);

  // Set the context menu items
  void set_menu(const std::vector<MenuItem>& items);

  // Handle window messages (call from window proc)
  // Returns true if the message was handled
  bool handle_message(UINT msg, WPARAM wparam, LPARAM lparam);

  // Get the custom message ID for tray icon events
  static UINT get_callback_message();

  // Load icons from resources or default system icons
  static HICON load_icon_disconnected();
  static HICON load_icon_connecting();
  static HICON load_icon_connected();
  static HICON load_icon_error();

 private:
  void update_icon();
  void show_context_menu();

  HWND window_{nullptr};
  NOTIFYICONDATAA nid_{};
  bool initialized_{false};
  ConnectionState state_{ConnectionState::kDisconnected};
  std::vector<MenuItem> menu_items_;

  // Cached icons
  HICON icon_disconnected_{nullptr};
  HICON icon_connecting_{nullptr};
  HICON icon_connected_{nullptr};
  HICON icon_error_{nullptr};

  // Animation timer for connecting state
  UINT_PTR animation_timer_{0};
  int animation_frame_{0};
};

// ============================================================================
// System Tray Manager (Singleton)
// ============================================================================
// Manages system tray integration across the application.

class SystemTrayManager {
 public:
  static SystemTrayManager& instance();

  // Initialize with connection callbacks
  void init(HWND window, std::function<void()> connect_callback,
            std::function<void()> disconnect_callback,
            std::function<void()> settings_callback,
            std::function<void()> exit_callback);

  // Update connection state
  void set_connected(bool connected);
  void set_connecting();
  void set_error(const std::string& message);

  // Update statistics (for tooltip)
  void update_stats(uint64_t bytes_sent, uint64_t bytes_received);

  // Show main window
  void show_main_window();

  // Get the tray object
  SystemTray& tray() { return tray_; }

  // Handle messages
  bool handle_message(UINT msg, WPARAM wparam, LPARAM lparam);

 private:
  SystemTrayManager() = default;

  void update_menu();
  std::string format_bytes(uint64_t bytes);

  SystemTray tray_;
  bool connected_{false};
  bool connecting_{false};
  std::string error_message_;
  uint64_t bytes_sent_{0};
  uint64_t bytes_received_{0};

  std::function<void()> connect_callback_;
  std::function<void()> disconnect_callback_;
  std::function<void()> settings_callback_;
  std::function<void()> exit_callback_;
  HWND main_window_{nullptr};
};

}  // namespace veil::windows

#endif  // _WIN32
