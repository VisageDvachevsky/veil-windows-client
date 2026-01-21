#ifdef _WIN32

#include "system_tray.h"

#include <sstream>
#include <iomanip>

#include "../common/logging/logger.h"

namespace veil::windows {

// Custom message for tray icon events
static constexpr UINT WM_TRAYICON = WM_USER + 100;

// Timer ID for connecting animation
static constexpr UINT_PTR TIMER_ANIMATION = 1;

// ============================================================================
// SystemTray Implementation
// ============================================================================

SystemTray::SystemTray() = default;

SystemTray::~SystemTray() {
  cleanup();
}

bool SystemTray::init(HWND window, const std::string& tooltip) {
  window_ = window;

  // Load icons
  icon_disconnected_ = load_icon_disconnected();
  icon_connecting_ = load_icon_connecting();
  icon_connected_ = load_icon_connected();
  icon_error_ = load_icon_error();

  // Initialize NOTIFYICONDATA structure
  ZeroMemory(&nid_, sizeof(nid_));
  nid_.cbSize = sizeof(NOTIFYICONDATAA);
  nid_.hWnd = window;
  nid_.uID = 1;
  nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
  nid_.uCallbackMessage = WM_TRAYICON;
  nid_.hIcon = icon_disconnected_;
  nid_.uVersion = NOTIFYICON_VERSION_4;

  // Set tooltip
  strncpy_s(nid_.szTip, tooltip.c_str(), sizeof(nid_.szTip) - 1);

  // Add the icon to the system tray
  if (!Shell_NotifyIconA(NIM_ADD, &nid_)) {
    LOG_ERROR("Failed to add system tray icon: {}", GetLastError());
    return false;
  }

  // Set the version for modern behavior
  Shell_NotifyIconA(NIM_SETVERSION, &nid_);

  initialized_ = true;
  LOG_INFO("System tray icon initialized");
  return true;
}

void SystemTray::cleanup() {
  if (initialized_) {
    // Stop animation timer
    if (animation_timer_) {
      KillTimer(window_, animation_timer_);
      animation_timer_ = 0;
    }

    // Remove the tray icon
    Shell_NotifyIconA(NIM_DELETE, &nid_);
    initialized_ = false;

    LOG_INFO("System tray icon removed");
  }

  // Clean up icons
  if (icon_disconnected_) DestroyIcon(icon_disconnected_);
  if (icon_connecting_) DestroyIcon(icon_connecting_);
  if (icon_connected_) DestroyIcon(icon_connected_);
  if (icon_error_) DestroyIcon(icon_error_);

  icon_disconnected_ = nullptr;
  icon_connecting_ = nullptr;
  icon_connected_ = nullptr;
  icon_error_ = nullptr;
}

void SystemTray::set_state(ConnectionState state) {
  if (state_ == state) {
    return;
  }

  state_ = state;

  // Handle animation timer
  if (state == ConnectionState::kConnecting) {
    animation_frame_ = 0;
    animation_timer_ = SetTimer(window_, TIMER_ANIMATION, 500, nullptr);
  } else if (animation_timer_) {
    KillTimer(window_, animation_timer_);
    animation_timer_ = 0;
  }

  update_icon();
}

void SystemTray::set_tooltip(const std::string& tooltip) {
  strncpy_s(nid_.szTip, tooltip.c_str(), sizeof(nid_.szTip) - 1);
  nid_.uFlags = NIF_TIP;
  Shell_NotifyIconA(NIM_MODIFY, &nid_);
}

void SystemTray::show_notification(const std::string& title,
                                   const std::string& message, DWORD icon) {
  nid_.uFlags = NIF_INFO;
  strncpy_s(nid_.szInfoTitle, title.c_str(), sizeof(nid_.szInfoTitle) - 1);
  strncpy_s(nid_.szInfo, message.c_str(), sizeof(nid_.szInfo) - 1);
  nid_.dwInfoFlags = icon;
  Shell_NotifyIconA(NIM_MODIFY, &nid_);
}

void SystemTray::set_menu(const std::vector<MenuItem>& items) {
  menu_items_ = items;
}

bool SystemTray::handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == WM_TRAYICON) {
    UINT event = LOWORD(lparam);

    switch (event) {
      case WM_LBUTTONUP:
        // Left click - show main window
        if (wparam == 1) {
          // Trigger first menu item (usually "Show")
          if (!menu_items_.empty() && menu_items_[0].callback) {
            menu_items_[0].callback();
          }
        }
        return true;

      case WM_RBUTTONUP:
        // Right click - show context menu
        show_context_menu();
        return true;

      case NIN_BALLOONUSERCLICK:
        // User clicked on balloon notification
        // Show main window
        if (!menu_items_.empty() && menu_items_[0].callback) {
          menu_items_[0].callback();
        }
        return true;
    }
  } else if (msg == WM_TIMER && wparam == TIMER_ANIMATION) {
    // Animation timer for connecting state
    animation_frame_ = (animation_frame_ + 1) % 4;
    update_icon();
    return true;
  }

  return false;
}

UINT SystemTray::get_callback_message() {
  return WM_TRAYICON;
}

void SystemTray::update_icon() {
  HICON icon = nullptr;

  switch (state_) {
    case ConnectionState::kDisconnected:
      icon = icon_disconnected_;
      break;
    case ConnectionState::kConnecting:
      // Animate between disconnected and connecting icons
      icon = (animation_frame_ % 2 == 0) ? icon_disconnected_ : icon_connecting_;
      break;
    case ConnectionState::kConnected:
      icon = icon_connected_;
      break;
    case ConnectionState::kError:
      icon = icon_error_;
      break;
  }

  if (icon && icon != nid_.hIcon) {
    nid_.hIcon = icon;
    nid_.uFlags = NIF_ICON;
    Shell_NotifyIconA(NIM_MODIFY, &nid_);
  }
}

void SystemTray::show_context_menu() {
  HMENU menu = CreatePopupMenu();
  if (!menu) {
    return;
  }

  // Add menu items
  int id = 1;
  for (const auto& item : menu_items_) {
    if (item.separator) {
      AppendMenuA(menu, MF_SEPARATOR, 0, nullptr);
    } else {
      UINT flags = MF_STRING;
      if (!item.enabled) {
        flags |= MF_GRAYED;
      }
      if (item.checked) {
        flags |= MF_CHECKED;
      }
      AppendMenuA(menu, flags, id, item.text.c_str());
    }
    id++;
  }

  // Get cursor position
  POINT pt;
  GetCursorPos(&pt);

  // Set foreground window (required for proper menu behavior)
  SetForegroundWindow(window_);

  // Show the menu
  int selected = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY,
                                  pt.x, pt.y, window_, nullptr);

  // Clean up
  DestroyMenu(menu);

  // Handle selection
  if (selected > 0 && selected <= static_cast<int>(menu_items_.size())) {
    const auto& item = menu_items_[selected - 1];
    if (item.callback && item.enabled) {
      item.callback();
    }
  }

  // Post a null message to close the menu properly
  PostMessage(window_, WM_NULL, 0, 0);
}

// ============================================================================
// Icon Loading
// ============================================================================

HICON SystemTray::load_icon_disconnected() {
  // Try to load from resources first
  HICON icon = LoadIconA(GetModuleHandle(nullptr), "IDI_DISCONNECTED");
  if (icon) {
    return icon;
  }

  // Fall back to system icon (gray shield)
  return LoadIconA(nullptr, IDI_APPLICATION);
}

HICON SystemTray::load_icon_connecting() {
  HICON icon = LoadIconA(GetModuleHandle(nullptr), "IDI_CONNECTING");
  if (icon) {
    return icon;
  }

  // Fall back to system icon (question)
  return LoadIconA(nullptr, IDI_QUESTION);
}

HICON SystemTray::load_icon_connected() {
  HICON icon = LoadIconA(GetModuleHandle(nullptr), "IDI_CONNECTED");
  if (icon) {
    return icon;
  }

  // Fall back to system icon (shield)
  return LoadIconA(nullptr, IDI_SHIELD);
}

HICON SystemTray::load_icon_error() {
  HICON icon = LoadIconA(GetModuleHandle(nullptr), "IDI_ERROR");
  if (icon) {
    return icon;
  }

  // Fall back to system error icon
  return LoadIconA(nullptr, IDI_ERROR);
}

// ============================================================================
// SystemTrayManager Implementation
// ============================================================================

SystemTrayManager& SystemTrayManager::instance() {
  static SystemTrayManager instance;
  return instance;
}

void SystemTrayManager::init(HWND window,
                             std::function<void()> connect_callback,
                             std::function<void()> disconnect_callback,
                             std::function<void()> settings_callback,
                             std::function<void()> exit_callback) {
  main_window_ = window;
  connect_callback_ = std::move(connect_callback);
  disconnect_callback_ = std::move(disconnect_callback);
  settings_callback_ = std::move(settings_callback);
  exit_callback_ = std::move(exit_callback);

  tray_.init(window, "VEIL VPN - Disconnected");
  update_menu();
}

void SystemTrayManager::set_connected(bool connected) {
  connected_ = connected;
  connecting_ = false;
  error_message_.clear();

  if (connected) {
    tray_.set_state(SystemTray::ConnectionState::kConnected);
    tray_.set_tooltip("VEIL VPN - Connected");
    tray_.show_notification("VEIL VPN", "Connected to VPN server", NIIF_INFO);
  } else {
    tray_.set_state(SystemTray::ConnectionState::kDisconnected);
    tray_.set_tooltip("VEIL VPN - Disconnected");
  }

  update_menu();
}

void SystemTrayManager::set_connecting() {
  connecting_ = true;
  error_message_.clear();
  tray_.set_state(SystemTray::ConnectionState::kConnecting);
  tray_.set_tooltip("VEIL VPN - Connecting...");
  update_menu();
}

void SystemTrayManager::set_error(const std::string& message) {
  connected_ = false;
  connecting_ = false;
  error_message_ = message;
  tray_.set_state(SystemTray::ConnectionState::kError);
  tray_.set_tooltip("VEIL VPN - Error: " + message);
  tray_.show_notification("VEIL VPN", "Error: " + message, NIIF_ERROR);
  update_menu();
}

void SystemTrayManager::update_stats(uint64_t bytes_sent,
                                     uint64_t bytes_received) {
  bytes_sent_ = bytes_sent;
  bytes_received_ = bytes_received;

  if (connected_) {
    std::stringstream tooltip;
    tooltip << "VEIL VPN - Connected\n"
            << "Sent: " << format_bytes(bytes_sent) << "\n"
            << "Received: " << format_bytes(bytes_received);
    tray_.set_tooltip(tooltip.str());
  }
}

void SystemTrayManager::show_main_window() {
  if (main_window_) {
    ShowWindow(main_window_, SW_SHOW);
    SetForegroundWindow(main_window_);
  }
}

bool SystemTrayManager::handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
  return tray_.handle_message(msg, wparam, lparam);
}

void SystemTrayManager::update_menu() {
  std::vector<SystemTray::MenuItem> items;

  // Show main window
  items.push_back({"Show VEIL VPN", [this]() { show_main_window(); }, true, false, false});

  items.push_back({"", nullptr, true, false, true});  // Separator

  // Connect/Disconnect
  if (connected_) {
    items.push_back({"Disconnect", disconnect_callback_, true, false, false});
  } else if (connecting_) {
    items.push_back({"Connecting...", nullptr, false, false, false});
  } else {
    items.push_back({"Connect", connect_callback_, true, false, false});
  }

  items.push_back({"", nullptr, true, false, true});  // Separator

  // Settings
  items.push_back({"Settings...", settings_callback_, true, false, false});

  items.push_back({"", nullptr, true, false, true});  // Separator

  // Exit
  items.push_back({"Exit", exit_callback_, true, false, false});

  tray_.set_menu(items);
}

std::string SystemTrayManager::format_bytes(uint64_t bytes) {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(1);

  if (bytes >= 1024ULL * 1024 * 1024) {
    ss << (static_cast<double>(bytes) / (1024.0 * 1024 * 1024)) << " GB";
  } else if (bytes >= 1024 * 1024) {
    ss << (static_cast<double>(bytes) / (1024.0 * 1024)) << " MB";
  } else if (bytes >= 1024) {
    ss << (static_cast<double>(bytes) / 1024.0) << " KB";
  } else {
    ss << bytes << " B";
  }

  return ss.str();
}

}  // namespace veil::windows

#endif  // _WIN32
