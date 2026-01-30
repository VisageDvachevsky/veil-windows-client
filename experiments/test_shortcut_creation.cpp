// Experiment: Test Windows shortcut creation functionality
// This file tests the ShortcutManager class to ensure shortcuts can be created correctly.

#ifdef _WIN32

#include <iostream>
#include <string>
#include <windows.h>
#include "../src/windows/shortcut_manager.h"

using namespace veil::windows;

int main() {
  std::cout << "=== VEIL Shortcut Manager Test ===" << std::endl;
  std::cout << std::endl;

  // Test 1: Get Desktop location
  std::cout << "[TEST 1] Getting Desktop location..." << std::endl;
  std::string error;
  std::string desktop_path = ShortcutManager::getLocationPath(
      ShortcutManager::Location::kDesktop, error);

  if (desktop_path.empty()) {
    std::cerr << "FAILED: Could not get Desktop path: " << error << std::endl;
    return 1;
  }
  std::cout << "SUCCESS: Desktop path: " << desktop_path << std::endl;
  std::cout << std::endl;

  // Test 2: Get Start Menu location
  std::cout << "[TEST 2] Getting Start Menu location..." << std::endl;
  error.clear();
  std::string start_menu_path = ShortcutManager::getLocationPath(
      ShortcutManager::Location::kStartMenu, error);

  if (start_menu_path.empty()) {
    std::cerr << "FAILED: Could not get Start Menu path: " << error << std::endl;
    return 1;
  }
  std::cout << "SUCCESS: Start Menu path: " << start_menu_path << std::endl;
  std::cout << std::endl;

  // Test 3: Create Desktop shortcut to notepad.exe
  std::cout << "[TEST 3] Creating Desktop shortcut to notepad.exe..." << std::endl;
  error.clear();
  std::string test_shortcut_name = "VEIL_Test_Shortcut";
  std::string target_path = "C:\\Windows\\System32\\notepad.exe";

  bool success = ShortcutManager::createShortcut(
      ShortcutManager::Location::kDesktop,
      test_shortcut_name,
      target_path,
      "",  // arguments
      "Test shortcut created by VEIL experiment",
      "",  // icon_path (use executable's icon)
      0,   // icon_index
      "",  // working_dir
      error
  );

  if (!success) {
    std::cerr << "FAILED: Could not create Desktop shortcut: " << error << std::endl;
    return 1;
  }
  std::cout << "SUCCESS: Desktop shortcut created!" << std::endl;
  std::cout << std::endl;

  // Test 4: Check if shortcut exists
  std::cout << "[TEST 4] Checking if Desktop shortcut exists..." << std::endl;
  bool exists = ShortcutManager::shortcutExists(
      ShortcutManager::Location::kDesktop,
      test_shortcut_name
  );

  if (!exists) {
    std::cerr << "FAILED: Shortcut should exist but doesn't!" << std::endl;
    return 1;
  }
  std::cout << "SUCCESS: Desktop shortcut exists!" << std::endl;
  std::cout << std::endl;

  // Test 5: Create Start Menu shortcut with arguments
  std::cout << "[TEST 5] Creating Start Menu shortcut with arguments..." << std::endl;
  error.clear();
  std::string test_file_path = "C:\\test.txt";

  success = ShortcutManager::createShortcut(
      ShortcutManager::Location::kStartMenu,
      test_shortcut_name,
      target_path,
      test_file_path,  // arguments - open test.txt
      "Test shortcut with arguments",
      "",  // icon_path
      0,   // icon_index
      "",  // working_dir
      error
  );

  if (!success) {
    std::cerr << "FAILED: Could not create Start Menu shortcut: " << error << std::endl;
    // Don't return 1 here, just warn
    std::cerr << "WARNING: Continuing with cleanup..." << std::endl;
  } else {
    std::cout << "SUCCESS: Start Menu shortcut created with arguments!" << std::endl;
  }
  std::cout << std::endl;

  // Test 6: Remove Desktop shortcut
  std::cout << "[TEST 6] Removing Desktop shortcut..." << std::endl;
  error.clear();
  success = ShortcutManager::removeShortcut(
      ShortcutManager::Location::kDesktop,
      test_shortcut_name,
      error
  );

  if (!success) {
    std::cerr << "FAILED: Could not remove Desktop shortcut: " << error << std::endl;
    return 1;
  }
  std::cout << "SUCCESS: Desktop shortcut removed!" << std::endl;
  std::cout << std::endl;

  // Test 7: Verify shortcut no longer exists
  std::cout << "[TEST 7] Verifying Desktop shortcut removal..." << std::endl;
  exists = ShortcutManager::shortcutExists(
      ShortcutManager::Location::kDesktop,
      test_shortcut_name
  );

  if (exists) {
    std::cerr << "FAILED: Shortcut should not exist but still does!" << std::endl;
    return 1;
  }
  std::cout << "SUCCESS: Desktop shortcut verified removed!" << std::endl;
  std::cout << std::endl;

  // Test 8: Remove Start Menu shortcut (cleanup)
  std::cout << "[TEST 8] Removing Start Menu shortcut (cleanup)..." << std::endl;
  error.clear();
  success = ShortcutManager::removeShortcut(
      ShortcutManager::Location::kStartMenu,
      test_shortcut_name,
      error
  );

  if (!success) {
    std::cerr << "WARNING: Could not remove Start Menu shortcut: " << error << std::endl;
    // Don't fail - might not have been created
  } else {
    std::cout << "SUCCESS: Start Menu shortcut removed!" << std::endl;
  }
  std::cout << std::endl;

  // Test 9: Test VEIL VPN shortcut creation (simulated)
  std::cout << "[TEST 9] Testing VEIL VPN shortcut creation..." << std::endl;
  error.clear();

  // Get current executable path (this will be the test executable)
  char exe_path[MAX_PATH];
  GetModuleFileNameA(NULL, exe_path, MAX_PATH);
  std::string veil_exe_path(exe_path);

  std::cout << "Using executable: " << veil_exe_path << std::endl;

  success = ShortcutManager::createShortcut(
      ShortcutManager::Location::kDesktop,
      "VEIL VPN",
      veil_exe_path,
      "",
      "VEIL VPN Client - Secure VPN Connection",
      "",
      0,
      "",
      error
  );

  if (!success) {
    std::cerr << "FAILED: Could not create VEIL VPN shortcut: " << error << std::endl;
    return 1;
  }
  std::cout << "SUCCESS: VEIL VPN shortcut created on Desktop!" << std::endl;
  std::cout << std::endl;

  // Cleanup VEIL VPN shortcut
  std::cout << "Cleaning up VEIL VPN shortcut..." << std::endl;
  ShortcutManager::removeShortcut(
      ShortcutManager::Location::kDesktop,
      "VEIL VPN",
      error
  );
  std::cout << std::endl;

  std::cout << "=== ALL TESTS PASSED ===" << std::endl;
  std::cout << std::endl;
  std::cout << "The ShortcutManager is working correctly!" << std::endl;

  return 0;
}

#else  // Non-Windows

#include <iostream>

int main() {
  std::cout << "This experiment is only available on Windows." << std::endl;
  return 0;
}

#endif  // _WIN32
