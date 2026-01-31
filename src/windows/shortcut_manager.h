#pragma once

#ifdef _WIN32

#include <string>

namespace veil::windows {

// ============================================================================
// Windows Shortcut Manager
// ============================================================================
// Provides functionality to create, remove, and manage Windows shortcuts
// in standard locations (Desktop, Start Menu, etc.)

class ShortcutManager {
 public:
  // Shortcut locations
  enum class Location {
    kDesktop,           // User's desktop (%USERPROFILE%\Desktop)
    kStartMenu,         // User's Start Menu programs folder
    kStartMenuCommon,   // All Users Start Menu (requires elevation)
  };

  // Create a shortcut to the specified executable
  // location: Where to create the shortcut
  // shortcut_name: Name of the shortcut (without .lnk extension)
  // target_path: Full path to the executable
  // error: Output parameter for error message
  // arguments: Optional command-line arguments
  // description: Optional shortcut description/tooltip
  // icon_path: Optional path to icon file (uses target_path if empty)
  // icon_index: Icon index within the icon file
  // working_dir: Optional working directory (uses target directory if empty)
  // Returns true on success
  static bool createShortcut(Location location,
                            const std::string& shortcut_name,
                            const std::string& target_path,
                            std::string& error,
                            const std::string& arguments = "",
                            const std::string& description = "",
                            const std::string& icon_path = "",
                            int icon_index = 0,
                            const std::string& working_dir = "");

  // Remove a shortcut
  // location: Where the shortcut is located
  // shortcut_name: Name of the shortcut (without .lnk extension)
  // error: Output parameter for error message
  // Returns true on success (or if shortcut doesn't exist)
  static bool removeShortcut(Location location,
                            const std::string& shortcut_name,
                            std::string& error);

  // Check if a shortcut exists
  // location: Where to check for the shortcut
  // shortcut_name: Name of the shortcut (without .lnk extension)
  // Returns true if the shortcut exists
  static bool shortcutExists(Location location,
                            const std::string& shortcut_name);

  // Get the full path to a shortcut location
  // location: The location to get the path for
  // error: Output parameter for error message
  // Returns the full path, or empty string on error
  static std::string getLocationPath(Location location, std::string& error);

  // Pin application to taskbar (Windows 10+)
  // Note: This is a best-effort operation as Windows doesn't provide
  // a documented API for programmatic taskbar pinning.
  // target_path: Full path to the executable
  // Returns true if the operation was initiated successfully
  static bool pinToTaskbar(const std::string& target_path);

 private:
  // Get the full path to a shortcut file
  static std::string getShortcutPath(Location location,
                                    const std::string& shortcut_name,
                                    std::string& error);

  // Initialize COM (required for IShellLink)
  // Returns true if COM was initialized successfully
  static bool initializeCOM();

  // Uninitialize COM
  static void uninitializeCOM();
};

}  // namespace veil::windows

#endif  // _WIN32
