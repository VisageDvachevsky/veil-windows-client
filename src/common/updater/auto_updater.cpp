#include "auto_updater.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>

#include <nlohmann/json.hpp>

#include "../logging/logger.h"

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#include <shellapi.h>
#pragma comment(lib, "wininet.lib")
#else
#include <curl/curl.h>
#endif

namespace veil::updater {

// Current application version (should be set during build)
#ifndef VEIL_VERSION_MAJOR
#define VEIL_VERSION_MAJOR 1
#endif
#ifndef VEIL_VERSION_MINOR
#define VEIL_VERSION_MINOR 0
#endif
#ifndef VEIL_VERSION_PATCH
#define VEIL_VERSION_PATCH 0
#endif
#ifndef VEIL_VERSION_PRERELEASE
#define VEIL_VERSION_PRERELEASE ""
#endif

// ============================================================================
// Version Implementation
// ============================================================================

std::optional<Version> Version::parse(const std::string& version_string) {
  // Match patterns like "1.2.3" or "1.2.3-beta.1" or "v1.2.3"
  std::regex version_regex(R"(v?(\d+)\.(\d+)\.(\d+)(?:-(.+))?)");
  std::smatch match;

  if (!std::regex_match(version_string, match, version_regex)) {
    return std::nullopt;
  }

  Version v;
  v.major = std::stoi(match[1].str());
  v.minor = std::stoi(match[2].str());
  v.patch = std::stoi(match[3].str());

  if (match[4].matched) {
    v.prerelease = match[4].str();
  }

  return v;
}

std::string Version::to_string() const {
  std::stringstream ss;
  ss << major << "." << minor << "." << patch;
  if (!prerelease.empty()) {
    ss << "-" << prerelease;
  }
  return ss.str();
}

bool Version::operator<(const Version& other) const {
  if (major != other.major) return major < other.major;
  if (minor != other.minor) return minor < other.minor;
  if (patch != other.patch) return patch < other.patch;

  // Prerelease versions are less than release versions
  if (prerelease.empty() && !other.prerelease.empty()) return false;
  if (!prerelease.empty() && other.prerelease.empty()) return true;

  return prerelease < other.prerelease;
}

bool Version::operator>(const Version& other) const {
  return other < *this;
}

bool Version::operator==(const Version& other) const {
  return major == other.major && minor == other.minor &&
         patch == other.patch && prerelease == other.prerelease;
}

bool Version::operator<=(const Version& other) const {
  return !(other < *this);
}

bool Version::operator>=(const Version& other) const {
  return !(*this < other);
}

bool Version::operator!=(const Version& other) const {
  return !(*this == other);
}

// ============================================================================
// ReleaseInfo Implementation
// ============================================================================

std::optional<ReleaseAsset> ReleaseInfo::find_installer() const {
  // Look for Windows installer assets
  std::vector<std::string> patterns = {
      ".exe",   // NSIS installer
      ".msi",   // MSI installer
      "-setup", // Setup suffix
      "-win64", // Windows 64-bit
  };

  for (const auto& asset : assets) {
    std::string name_lower = asset.name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                   ::tolower);

    // Skip non-Windows assets
    if (name_lower.find("linux") != std::string::npos ||
        name_lower.find("macos") != std::string::npos ||
        name_lower.find("darwin") != std::string::npos) {
      continue;
    }

    for (const auto& pattern : patterns) {
      if (name_lower.find(pattern) != std::string::npos) {
        return asset;
      }
    }
  }

  return std::nullopt;
}

// ============================================================================
// HTTP Helper Functions
// ============================================================================

#ifdef _WIN32

// Windows HTTP implementation using WinINet
static std::string http_get(const std::string& url, std::string& error) {
  HINTERNET hInternet = InternetOpenA(
      "VEIL-VPN-Updater/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);

  if (!hInternet) {
    error = "Failed to initialize WinINet: " + std::to_string(GetLastError());
    return "";
  }

  HINTERNET hUrl = InternetOpenUrlA(
      hInternet, url.c_str(), nullptr, 0,
      INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
      0);

  if (!hUrl) {
    error = "Failed to open URL: " + std::to_string(GetLastError());
    InternetCloseHandle(hInternet);
    return "";
  }

  std::string response;
  char buffer[8192];
  DWORD bytes_read;

  while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytes_read) &&
         bytes_read > 0) {
    response.append(buffer, bytes_read);
  }

  InternetCloseHandle(hUrl);
  InternetCloseHandle(hInternet);

  return response;
}

static bool http_download(const std::string& url, const std::string& path,
                          std::function<void(size_t, size_t)> progress,
                          std::string& error) {
  HINTERNET hInternet = InternetOpenA(
      "VEIL-VPN-Updater/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);

  if (!hInternet) {
    error = "Failed to initialize WinINet: " + std::to_string(GetLastError());
    return false;
  }

  HINTERNET hUrl = InternetOpenUrlA(
      hInternet, url.c_str(), nullptr, 0,
      INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
      0);

  if (!hUrl) {
    error = "Failed to open URL: " + std::to_string(GetLastError());
    InternetCloseHandle(hInternet);
    return false;
  }

  // Get content length
  DWORD content_length = 0;
  DWORD size = sizeof(content_length);
  HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
                 &content_length, &size, nullptr);

  std::ofstream file(path, std::ios::binary);
  if (!file) {
    error = "Failed to create file: " + path;
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    return false;
  }

  char buffer[8192];
  DWORD bytes_read;
  size_t total_read = 0;

  while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytes_read) &&
         bytes_read > 0) {
    file.write(buffer, bytes_read);
    total_read += bytes_read;

    if (progress) {
      progress(total_read, content_length);
    }
  }

  file.close();
  InternetCloseHandle(hUrl);
  InternetCloseHandle(hInternet);

  return true;
}

#else

// Linux/Unix HTTP implementation using libcurl
static size_t write_callback(void* contents, size_t size, size_t nmemb,
                             std::string* response) {
  size_t total = size * nmemb;
  response->append(static_cast<char*>(contents), total);
  return total;
}

static std::string http_get(const std::string& url, std::string& error) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    error = "Failed to initialize libcurl";
    return "";
  }

  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "VEIL-VPN-Updater/1.0");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    error = curl_easy_strerror(res);
    response.clear();
  }

  curl_easy_cleanup(curl);
  return response;
}

struct DownloadProgress {
  std::function<void(size_t, size_t)>* callback;
};

static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                             curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
  auto* progress = static_cast<DownloadProgress*>(clientp);
  if (progress && progress->callback && *progress->callback) {
    (*progress->callback)(static_cast<size_t>(dlnow),
                          static_cast<size_t>(dltotal));
  }
  return 0;
}

static bool http_download(const std::string& url, const std::string& path,
                          std::function<void(size_t, size_t)> progress,
                          std::string& error) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    error = "Failed to initialize libcurl";
    return false;
  }

  FILE* file = fopen(path.c_str(), "wb");
  if (!file) {
    error = "Failed to create file: " + path;
    curl_easy_cleanup(curl);
    return false;
  }

  DownloadProgress progress_data{&progress};

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "VEIL-VPN-Updater/1.0");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

  CURLcode res = curl_easy_perform(curl);
  fclose(file);

  if (res != CURLE_OK) {
    error = curl_easy_strerror(res);
    curl_easy_cleanup(curl);
    return false;
  }

  curl_easy_cleanup(curl);
  return true;
}

#endif

// ============================================================================
// AutoUpdater Implementation
// ============================================================================

struct AutoUpdater::Impl {
  std::string last_check_time;
};

AutoUpdater::AutoUpdater(UpdateConfig config)
    : impl_(std::make_unique<Impl>()), config_(std::move(config)) {}

AutoUpdater::~AutoUpdater() = default;

Version AutoUpdater::current_version() {
  Version v;
  v.major = VEIL_VERSION_MAJOR;
  v.minor = VEIL_VERSION_MINOR;
  v.patch = VEIL_VERSION_PATCH;
  v.prerelease = VEIL_VERSION_PRERELEASE;
  return v;
}

void AutoUpdater::check_for_updates(CheckCallback callback) {
  std::thread([this, callback]() {
    auto release = check_for_updates_sync();
    if (callback) {
      callback(release.has_value(), release.value_or(ReleaseInfo{}));
    }
  }).detach();
}

std::optional<ReleaseInfo> AutoUpdater::check_for_updates_sync() {
  std::string url;

  if (!config_.custom_update_url.empty()) {
    url = config_.custom_update_url;
  } else {
    // GitHub API for latest release
    url = "https://api.github.com/repos/" + config_.github_owner + "/" +
          config_.github_repo + "/releases/latest";
  }

  LOG_DEBUG("Checking for updates at: {}", url);

  std::string error;
  std::string response = http_get(url, error);

  if (response.empty()) {
    LOG_ERROR("Failed to check for updates: {}", error);
    if (error_callback_) {
      error_callback_(error);
    }
    return std::nullopt;
  }

  try {
    auto json = nlohmann::json::parse(response);

    ReleaseInfo release;
    release.tag_name = json.value("tag_name", "");
    release.name = json.value("name", "");
    release.body = json.value("body", "");
    release.published_at = json.value("published_at", "");
    release.html_url = json.value("html_url", "");
    release.prerelease = json.value("prerelease", false);
    release.draft = json.value("draft", false);

    // Parse version from tag
    auto version = Version::parse(release.tag_name);
    if (!version) {
      LOG_WARN("Failed to parse version from tag: {}", release.tag_name);
      return std::nullopt;
    }
    release.version = *version;

    // Parse assets
    if (json.contains("assets") && json["assets"].is_array()) {
      for (const auto& asset : json["assets"]) {
        ReleaseAsset ra;
        ra.name = asset.value("name", "");
        ra.download_url = asset.value("browser_download_url", "");
        ra.content_type = asset.value("content_type", "");
        ra.size = asset.value("size", 0);
        release.assets.push_back(ra);
      }
    }

    // Update last check time
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    impl_->last_check_time = std::ctime(&time);

    // Check if this is a newer version
    Version current = current_version();

    // Skip prereleases unless configured to check them
    if (release.prerelease && !config_.check_for_prereleases) {
      LOG_DEBUG("Skipping prerelease: {}", release.tag_name);
      return std::nullopt;
    }

    // Check if ignored
    if (is_version_ignored(release.version)) {
      LOG_DEBUG("Skipping ignored version: {}", release.tag_name);
      return std::nullopt;
    }

    if (release.version > current) {
      LOG_INFO("Update available: {} -> {}", current.to_string(),
               release.version.to_string());
      cached_release_ = release;
      return release;
    }

    LOG_DEBUG("No update available (current: {}, latest: {})",
              current.to_string(), release.version.to_string());
    return std::nullopt;

  } catch (const std::exception& e) {
    LOG_ERROR("Failed to parse update response: {}", e.what());
    if (error_callback_) {
      error_callback_(std::string("Parse error: ") + e.what());
    }
    return std::nullopt;
  }
}

void AutoUpdater::download_update(const ReleaseInfo& release,
                                  DownloadProgressCallback progress_callback,
                                  DownloadCompleteCallback complete_callback) {
  std::thread([this, release, progress_callback, complete_callback]() {
    auto installer = release.find_installer();
    if (!installer) {
      if (complete_callback) {
        complete_callback(false, "No installer found for this platform");
      }
      return;
    }

    // Determine download path
    std::string download_dir = config_.download_directory;
    if (download_dir.empty()) {
#ifdef _WIN32
      char temp_path[MAX_PATH];
      GetTempPathA(MAX_PATH, temp_path);
      download_dir = temp_path;
#else
      download_dir = "/tmp";
#endif
    }

    std::string download_path = download_dir + "/" + installer->name;

    LOG_INFO("Downloading update: {} -> {}", installer->download_url,
             download_path);

    std::string error;
    bool success = http_download(installer->download_url, download_path,
                                 progress_callback, error);

    if (complete_callback) {
      if (success) {
        complete_callback(true, download_path);
      } else {
        complete_callback(false, error);
      }
    }
  }).detach();
}

bool AutoUpdater::install_update(const std::string& installer_path,
                                 std::string& error) {
#ifdef _WIN32
  // Use ShellExecute to run the installer with elevation
  SHELLEXECUTEINFOA sei = {};
  sei.cbSize = sizeof(sei);
  sei.lpVerb = "runas";  // Request elevation
  sei.lpFile = installer_path.c_str();
  sei.nShow = SW_SHOWNORMAL;
  sei.fMask = SEE_MASK_NOCLOSEPROCESS;

  if (!ShellExecuteExA(&sei)) {
    error = "Failed to launch installer: " + std::to_string(GetLastError());
    return false;
  }

  // Exit the current application to allow the installer to proceed
  LOG_INFO("Installer launched, exiting application");
  ExitProcess(0);
#else
  // On Linux, we might use a different approach (e.g., package manager)
  LOG_WARN("Auto-installation not implemented for this platform");
  error = "Auto-installation not implemented for this platform";
  return false;
#endif

  return true;
}

std::optional<ReleaseInfo> AutoUpdater::get_cached_release() const {
  return cached_release_;
}

void AutoUpdater::on_error(ErrorCallback callback) {
  error_callback_ = std::move(callback);
}

std::string AutoUpdater::get_last_check_time() const {
  return impl_->last_check_time;
}

void AutoUpdater::ignore_version(const Version& version) {
  if (!is_version_ignored(version)) {
    ignored_versions_.push_back(version);
  }
}

bool AutoUpdater::is_version_ignored(const Version& version) const {
  return std::find(ignored_versions_.begin(), ignored_versions_.end(),
                   version) != ignored_versions_.end();
}

// ============================================================================
// Update Dialog (stub - implementation in GUI module)
// ============================================================================

std::optional<UpdateDialogResult> show_update_dialog(
    const ReleaseInfo& /*release*/, const Version& /*current_version*/,
    bool /*already_downloaded*/) {
  // This is a stub - the actual implementation is in the Qt GUI module
  LOG_WARN("show_update_dialog() called but GUI not available");
  return std::nullopt;
}

}  // namespace veil::updater
