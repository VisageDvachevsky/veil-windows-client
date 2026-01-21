#pragma once

#include <string>
#include <iostream>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace veil::cli {

// ANSI color codes
namespace colors {
constexpr const char* kReset = "\033[0m";
constexpr const char* kBold = "\033[1m";
constexpr const char* kDim = "\033[2m";
constexpr const char* kUnderline = "\033[4m";

// Foreground colors
constexpr const char* kBlack = "\033[30m";
constexpr const char* kRed = "\033[31m";
constexpr const char* kGreen = "\033[32m";
constexpr const char* kYellow = "\033[33m";
constexpr const char* kBlue = "\033[34m";
constexpr const char* kMagenta = "\033[35m";
constexpr const char* kCyan = "\033[36m";
constexpr const char* kWhite = "\033[37m";

// Bright foreground colors
constexpr const char* kBrightBlack = "\033[90m";
constexpr const char* kBrightRed = "\033[91m";
constexpr const char* kBrightGreen = "\033[92m";
constexpr const char* kBrightYellow = "\033[93m";
constexpr const char* kBrightBlue = "\033[94m";
constexpr const char* kBrightMagenta = "\033[95m";
constexpr const char* kBrightCyan = "\033[96m";
constexpr const char* kBrightWhite = "\033[97m";

// Background colors
constexpr const char* kBgBlack = "\033[40m";
constexpr const char* kBgRed = "\033[41m";
constexpr const char* kBgGreen = "\033[42m";
constexpr const char* kBgYellow = "\033[43m";
constexpr const char* kBgBlue = "\033[44m";
constexpr const char* kBgMagenta = "\033[45m";
constexpr const char* kBgCyan = "\033[46m";
constexpr const char* kBgWhite = "\033[47m";
}  // namespace colors

// Unicode symbols for CLI
namespace symbols {
constexpr const char* kCheckmark = "\u2714";      // ✔
constexpr const char* kCross = "\u2718";          // ✘
constexpr const char* kArrowRight = "\u2192";     // →
constexpr const char* kArrowLeft = "\u2190";      // ←
constexpr const char* kArrowUp = "\u2191";        // ↑
constexpr const char* kArrowDown = "\u2193";      // ↓
constexpr const char* kDot = "\u2022";            // •
constexpr const char* kCircle = "\u25CF";         // ●
constexpr const char* kCircleEmpty = "\u25CB";    // ○
constexpr const char* kSquare = "\u25A0";         // ■
constexpr const char* kSquareEmpty = "\u25A1";    // □
constexpr const char* kWarning = "\u26A0";        // ⚠
constexpr const char* kInfo = "\u2139";           // ℹ
constexpr const char* kStar = "\u2605";           // ★
constexpr const char* kSpinner[] = {"\u280B", "\u2819", "\u2839", "\u2838",
                                     "\u283C", "\u2834", "\u2826", "\u2827",
                                     "\u2807", "\u280F"};  // Braille spinner
constexpr int kSpinnerFrames = 10;

// Simple ASCII fallbacks
constexpr const char* kCheckmarkAscii = "[OK]";
constexpr const char* kCrossAscii = "[FAIL]";
constexpr const char* kArrowRightAscii = "->";
constexpr const char* kDotAscii = "*";
constexpr const char* kWarningAscii = "[!]";
constexpr const char* kInfoAscii = "[i]";
}  // namespace symbols

// Check if terminal supports colors
inline bool supports_color() {
#ifdef _WIN32
  // Enable ANSI on Windows 10+
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwMode = 0;
  if (GetConsoleMode(hOut, &dwMode)) {
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    return true;
  }
  return false;
#else
  // Check for TTY and TERM environment
  if (isatty(fileno(stdout)) == 0) {
    return false;
  }
  const char* term = std::getenv("TERM");
  if (term == nullptr) {
    return false;
  }
  std::string term_str(term);
  return term_str != "dumb" && term_str != "";
#endif
}

// Check if terminal supports Unicode
inline bool supports_unicode() {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  return true;
#else
  const char* lang = std::getenv("LANG");
  const char* lc_all = std::getenv("LC_ALL");
  const char* lc_ctype = std::getenv("LC_CTYPE");

  auto check_utf8 = [](const char* env) {
    if (env == nullptr) return false;
    std::string s(env);
    return s.find("UTF-8") != std::string::npos ||
           s.find("utf-8") != std::string::npos ||
           s.find("utf8") != std::string::npos;
  };

  return check_utf8(lc_all) || check_utf8(lc_ctype) || check_utf8(lang);
#endif
}

// Global state for color/unicode support
struct CliState {
  bool use_color = supports_color();
  bool use_unicode = supports_unicode();
};

inline CliState& cli_state() {
  static CliState state;
  return state;
}

// Colorize text
inline std::string colorize(const std::string& text, const char* color) {
  if (!cli_state().use_color) {
    return text;
  }
  return std::string(color) + text + colors::kReset;
}

// Status message helpers
inline void print_success(const std::string& message) {
  auto& state = cli_state();
  if (state.use_color) {
    std::cout << colors::kBrightGreen;
  }
  std::cout << (state.use_unicode ? symbols::kCheckmark : symbols::kCheckmarkAscii);
  if (state.use_color) {
    std::cout << colors::kReset;
  }
  std::cout << " " << message << '\n';
}

inline void print_error(const std::string& message) {
  auto& state = cli_state();
  if (state.use_color) {
    std::cerr << colors::kBrightRed;
  }
  std::cerr << (state.use_unicode ? symbols::kCross : symbols::kCrossAscii);
  if (state.use_color) {
    std::cerr << colors::kReset;
  }
  std::cerr << " " << message << '\n';
}

inline void print_warning(const std::string& message) {
  auto& state = cli_state();
  if (state.use_color) {
    std::cerr << colors::kBrightYellow;
  }
  std::cerr << (state.use_unicode ? symbols::kWarning : symbols::kWarningAscii);
  if (state.use_color) {
    std::cerr << colors::kReset;
  }
  std::cerr << " " << message << '\n';
}

inline void print_info(const std::string& message) {
  auto& state = cli_state();
  if (state.use_color) {
    std::cout << colors::kBrightCyan;
  }
  std::cout << (state.use_unicode ? symbols::kInfo : symbols::kInfoAscii);
  if (state.use_color) {
    std::cout << colors::kReset;
  }
  std::cout << " " << message << '\n';
}

inline void print_status(const std::string& label, const std::string& value,
                         const char* value_color = nullptr) {
  auto& state = cli_state();
  if (state.use_color) {
    std::cout << colors::kDim;
  }
  std::cout << label << ": ";
  if (state.use_color) {
    std::cout << colors::kReset;
    if (value_color != nullptr) {
      std::cout << value_color;
    }
  }
  std::cout << value;
  if (state.use_color && value_color != nullptr) {
    std::cout << colors::kReset;
  }
  std::cout << '\n';
}

// Progress bar
class ProgressBar {
 public:
  explicit ProgressBar(int total, int width = 40, std::string prefix = "")
      : total_(total), width_(width), prefix_(std::move(prefix)) {}

  void update(int current) {
    current_ = current;
    render();
  }

  void increment() { update(current_ + 1); }

  void finish() {
    update(total_);
    std::cout << '\n';
  }

 private:
  void render() {
    auto& state = cli_state();
    float progress = static_cast<float>(current_) / static_cast<float>(total_);
    int filled = static_cast<int>(static_cast<float>(width_) * progress);

    std::cout << "\r";
    if (!prefix_.empty()) {
      std::cout << prefix_ << " ";
    }

    std::cout << "[";
    if (state.use_color) {
      std::cout << colors::kBrightCyan;
    }

    for (int i = 0; i < width_; ++i) {
      if (i < filled) {
        std::cout << (state.use_unicode ? "\u2588" : "#");  // █ or #
      } else {
        std::cout << (state.use_unicode ? "\u2591" : "-");  // ░ or -
      }
    }

    if (state.use_color) {
      std::cout << colors::kReset;
    }

    std::cout << "] " << static_cast<int>(progress * 100) << "%";
    std::cout.flush();
  }

  int total_;
  int width_;
  std::string prefix_;
  int current_{0};
};

// Spinner for ongoing operations
class Spinner {
 public:
  explicit Spinner(std::string message) : message_(std::move(message)) {}

  void start() {
    running_ = true;
    render();
  }

  void tick() {
    if (!running_) return;
    frame_ = (frame_ + 1) % symbols::kSpinnerFrames;
    render();
  }

  void stop(bool success = true) {
    running_ = false;
    auto& state = cli_state();

    std::cout << "\r";
    if (state.use_color) {
      std::cout << (success ? colors::kBrightGreen : colors::kBrightRed);
    }
    if (state.use_unicode) {
      std::cout << (success ? symbols::kCheckmark : symbols::kCross);
    } else {
      std::cout << (success ? symbols::kCheckmarkAscii : symbols::kCrossAscii);
    }
    if (state.use_color) {
      std::cout << colors::kReset;
    }
    std::cout << " " << message_ << "          " << '\n';  // Extra spaces to clear
  }

 private:
  void render() {
    auto& state = cli_state();
    std::cout << "\r";
    if (state.use_color) {
      std::cout << colors::kBrightCyan;
    }
    if (state.use_unicode) {
      std::cout << symbols::kSpinner[frame_];
    } else {
      const char* ascii_spinner = "|/-\\";
      std::cout << ascii_spinner[frame_ % 4];
    }
    if (state.use_color) {
      std::cout << colors::kReset;
    }
    std::cout << " " << message_;
    std::cout.flush();
  }

  std::string message_;
  int frame_{0};
  bool running_{false};
};

// Banner/header printing
inline void print_banner(const std::string& title, const std::string& version = "") {
  auto& state = cli_state();
  std::string line(title.length() + (version.empty() ? 0 : version.length() + 3) + 4, '=');

  if (state.use_color) {
    std::cout << colors::kBrightCyan << colors::kBold;
  }
  std::cout << line << '\n';
  std::cout << "  " << title;
  if (!version.empty()) {
    if (state.use_color) {
      std::cout << colors::kReset << colors::kDim;
    }
    std::cout << " v" << version;
    if (state.use_color) {
      std::cout << colors::kReset << colors::kBrightCyan << colors::kBold;
    }
  }
  std::cout << "  " << '\n';
  std::cout << line;
  if (state.use_color) {
    std::cout << colors::kReset;
  }
  std::cout << '\n';
}

// Section header
inline void print_section(const std::string& title) {
  auto& state = cli_state();
  std::cout << '\n';
  if (state.use_color) {
    std::cout << colors::kBold << colors::kBrightWhite;
  }
  std::cout << title;
  if (state.use_color) {
    std::cout << colors::kReset;
  }
  std::cout << '\n';

  if (state.use_color) {
    std::cout << colors::kDim;
  }
  std::cout << std::string(title.length(), '-');
  if (state.use_color) {
    std::cout << colors::kReset;
  }
  std::cout << '\n';
}

// Key-value table row
inline void print_row(const std::string& key, const std::string& value, int key_width = 20) {
  auto& state = cli_state();
  if (state.use_color) {
    std::cout << colors::kDim;
  }
  std::cout << "  ";
  std::cout.width(key_width);
  std::cout << std::left << key;
  if (state.use_color) {
    std::cout << colors::kReset;
  }
  std::cout << " : " << value << '\n';
}

// Key-value table row with colored value
inline void print_row_colored(const std::string& key, const std::string& value,
                               const char* value_color, int key_width = 20) {
  auto& state = cli_state();
  if (state.use_color) {
    std::cout << colors::kDim;
  }
  std::cout << "  ";
  std::cout.width(key_width);
  std::cout << std::left << key;
  if (state.use_color) {
    std::cout << colors::kReset;
  }
  std::cout << " : ";
  if (state.use_color && value_color != nullptr) {
    std::cout << value_color;
  }
  std::cout << value;
  if (state.use_color && value_color != nullptr) {
    std::cout << colors::kReset;
  }
  std::cout << '\n';
}

// Format bytes to human-readable
inline std::string format_bytes(uint64_t bytes) {
  const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  int unit_index = 0;
  double size = static_cast<double>(bytes);

  while (size >= 1024 && unit_index < 4) {
    size /= 1024;
    unit_index++;
  }

  char buf[32];
  if (unit_index == 0) {
    std::snprintf(buf, sizeof(buf), "%llu %s", static_cast<unsigned long long>(bytes),
                  units[unit_index]);
  } else {
    std::snprintf(buf, sizeof(buf), "%.2f %s", size, units[unit_index]);
  }
  return std::string(buf);
}

// Format duration in seconds to human-readable
inline std::string format_duration(int64_t seconds) {
  if (seconds < 60) {
    return std::to_string(seconds) + "s";
  } else if (seconds < 3600) {
    int mins = static_cast<int>(seconds / 60);
    int secs = static_cast<int>(seconds % 60);
    return std::to_string(mins) + "m " + std::to_string(secs) + "s";
  } else {
    int hours = static_cast<int>(seconds / 3600);
    int mins = static_cast<int>((seconds % 3600) / 60);
    return std::to_string(hours) + "h " + std::to_string(mins) + "m";
  }
}

}  // namespace veil::cli
