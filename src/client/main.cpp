#include <cstdlib>
#include <iostream>
#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>

#include "client/client_config.h"
#include "common/cli/cli_utils.h"
#ifndef _WIN32
#include "common/daemon/daemon.h"
#include "common/signal/signal_handler.h"
#endif
#include "common/logging/logger.h"
#include "tunnel/tunnel.h"
#include "tun/routing.h"

using namespace veil;

namespace {

// Connection state names with colors
const char* get_state_color(tunnel::ConnectionState state) {
  switch (state) {
    case tunnel::ConnectionState::kDisconnected:
      return cli::colors::kBrightRed;
    case tunnel::ConnectionState::kConnecting:
    case tunnel::ConnectionState::kHandshaking:
    case tunnel::ConnectionState::kReconnecting:
      return cli::colors::kBrightYellow;
    case tunnel::ConnectionState::kConnected:
      return cli::colors::kBrightGreen;
    default:
      return cli::colors::kReset;
  }
}

const char* get_state_name(tunnel::ConnectionState state) {
  switch (state) {
    case tunnel::ConnectionState::kDisconnected:
      return "Disconnected";
    case tunnel::ConnectionState::kConnecting:
      return "Connecting";
    case tunnel::ConnectionState::kHandshaking:
      return "Handshaking";
    case tunnel::ConnectionState::kConnected:
      return "Connected";
    case tunnel::ConnectionState::kReconnecting:
      return "Reconnecting";
    default:
      return "Unknown";
  }
}

// Log state change with colored output
void log_state_change(tunnel::ConnectionState old_state, tunnel::ConnectionState new_state) {
  LOG_INFO("State changed: {} -> {}", get_state_name(old_state), get_state_name(new_state));

  auto& cli = cli::cli_state();
  if (cli.use_color) {
    std::cout << cli::colors::kDim << "  State: " << cli::colors::kReset
              << get_state_color(old_state) << get_state_name(old_state) << cli::colors::kReset
              << " " << cli::colors::kDim << cli::symbols::kArrowRight << cli::colors::kReset
              << " " << get_state_color(new_state) << get_state_name(new_state)
              << cli::colors::kReset << '\n';
  } else {
    std::cout << "  State: " << get_state_name(old_state) << " -> " << get_state_name(new_state)
              << '\n';
  }
}

void log_tunnel_error(const std::string& error) {
  LOG_ERROR("Tunnel error: {}", error);
  cli::print_error("Tunnel error: " + error);
}

void log_signal_sigint() {
  LOG_INFO("Received SIGINT, shutting down...");
  std::cout << '\n';
  cli::print_warning("Received interrupt signal, shutting down gracefully...");
}

void log_signal_sigterm() {
  LOG_INFO("Received SIGTERM, shutting down...");
  std::cout << '\n';
  cli::print_warning("Received termination signal, shutting down gracefully...");
}

void print_configuration(const client::ClientConfig& config) {
  cli::print_section("Configuration");
  cli::print_row("Server", config.tunnel.server_address + ":" +
                               std::to_string(config.tunnel.server_port));
  cli::print_row("TUN Device", config.tunnel.tun.device_name);
  cli::print_row("TUN IP", config.tunnel.tun.ip_address + "/" + config.tunnel.tun.netmask);
  cli::print_row("MTU", std::to_string(config.tunnel.tun.mtu));
  cli::print_row("Verbose", config.verbose ? "Yes" : "No");
  cli::print_row("Daemon Mode", config.daemon_mode ? "Yes" : "No");
  cli::print_row("Default Route", config.set_default_route ? "Yes" : "No");

  if (!config.routes.empty()) {
    std::string routes_str;
    for (size_t i = 0; i < config.routes.size(); ++i) {
      if (i > 0) routes_str += ", ";
      routes_str += config.routes[i];
    }
    cli::print_row("Custom Routes", routes_str);
  }
  std::cout << '\n';
}

}  // namespace

int main(int argc, char* argv[]) {
  // Parse configuration
  client::ClientConfig config;
  std::error_code ec;

  if (!client::parse_args(argc, argv, config, ec)) {
    cli::print_error("Failed to parse arguments: " + ec.message());
    std::cerr << '\n';
    std::cerr << "Usage: veil-client -s <server> [-p <port>] [options]" << '\n';
    std::cerr << '\n';
    std::cerr << "Options:" << '\n';
    std::cerr << "  -s, --server <addr>    Server address (required)" << '\n';
    std::cerr << "  -p, --port <port>      Server port (default: 4433)" << '\n';
    std::cerr << "  -c, --config <file>    Configuration file path" << '\n';
    std::cerr << "  -k, --key <file>       Pre-shared key file" << '\n';
    std::cerr << "  -d, --daemon           Run as daemon" << '\n';
    std::cerr << "  -v, --verbose          Enable verbose logging" << '\n';
    std::cerr << "  --default-route        Set as default route" << '\n';
    std::cerr << "  --route <cidr>         Additional routes to add" << '\n';
    std::cerr << "  --tun-name <name>      TUN device name (default: veil0)" << '\n';
    std::cerr << "  --tun-ip <ip>          TUN device IP (default: 10.8.0.2)" << '\n';
    std::cerr << "  --mtu <size>           MTU size (default: 1400)" << '\n';
    std::cerr << '\n';
    return EXIT_FAILURE;
  }

  // Validate configuration
  std::string validation_error;
  if (!client::validate_config(config, validation_error)) {
    cli::print_error("Configuration error: " + validation_error);
    return EXIT_FAILURE;
  }

  // Print banner (only if not daemon mode)
  if (!config.daemon_mode) {
    cli::print_banner("VEIL VPN Client", "1.0.0");
    print_configuration(config);
  }

  // Initialize logging
  logging::configure_logging(config.verbose ? logging::LogLevel::debug : logging::LogLevel::info,
                              true);
  LOG_INFO("VEIL Client starting...");

#ifndef _WIN32
  // Check if already running (POSIX only)
  if (!config.pid_file.empty() && daemon::is_already_running(config.pid_file, ec)) {
    cli::print_error("Another instance is already running (PID file: " + config.pid_file + ")");
    return EXIT_FAILURE;
  }

  // Daemonize if requested (POSIX only)
  if (config.daemon_mode) {
    daemon::DaemonConfig daemon_config;
    daemon_config.pid_file = config.pid_file;
    daemon_config.user = config.user;
    daemon_config.group = config.group;

    cli::print_info("Daemonizing...");
    LOG_INFO("Daemonizing...");
    if (!daemon::daemonize(daemon_config, ec)) {
      cli::print_error("Failed to daemonize: " + ec.message());
      LOG_ERROR("Failed to daemonize: {}", ec.message());
      return EXIT_FAILURE;
    }
  }

  // Create PID file if not daemonizing (daemonize() creates it) (POSIX only)
  std::unique_ptr<daemon::PidFile> pid_file;
  if (!config.daemon_mode && !config.pid_file.empty()) {
    pid_file = std::make_unique<daemon::PidFile>(config.pid_file);
    if (!pid_file->create(ec)) {
      cli::print_warning("Failed to create PID file: " + ec.message());
      LOG_WARN("Failed to create PID file: {}", ec.message());
    }
  }
#endif

  // Create tunnel
  tunnel::Tunnel tun(config.tunnel);

  // Setup callbacks
  tun.on_state_change([](tunnel::ConnectionState old_state, tunnel::ConnectionState new_state) {
    log_state_change(old_state, new_state);
  });

  tun.on_error([](const std::string& error) { log_tunnel_error(error); });

  // Initialize tunnel
  cli::print_info("Initializing tunnel...");
  if (!tun.initialize(ec)) {
    cli::print_error("Failed to initialize tunnel: " + ec.message());
    LOG_ERROR("Failed to initialize tunnel: {}", ec.message());
    return EXIT_FAILURE;
  }
  cli::print_success("Tunnel initialized");

  // Setup routing
  auto* route_manager = tun.route_manager();

  // Add custom routes
  if (!config.routes.empty()) {
    cli::print_info("Setting up custom routes...");
  }
  for (const auto& route_str : config.routes) {
    tun::Route route;
    // Parse CIDR notation (e.g., "192.168.1.0/24")
    auto slash_pos = route_str.find('/');
    if (slash_pos != std::string::npos) {
      route.destination = route_str;  // Keep CIDR notation for ip route
    } else {
      route.destination = route_str;
      route.netmask = "255.255.255.255";
    }
    route.interface = tun.tun_device()->device_name();

    if (!route_manager->add_route(route, ec)) {
      cli::print_warning("Failed to add route " + route_str + ": " + ec.message());
      LOG_WARN("Failed to add route {}: {}", route_str, ec.message());
    } else {
      cli::print_success("Added route: " + route_str);
    }
  }

  // Set default route if requested
  if (config.set_default_route) {
    cli::print_info("Setting default route...");

    // Get current default gateway first for bypass route
    auto state = route_manager->get_system_state(ec);
    if (state && !state->default_gateway.empty()) {
      // Add bypass route for server address via original gateway
      tun::Route bypass;
      bypass.destination = config.tunnel.server_address;
      bypass.netmask = "255.255.255.255";
      bypass.gateway = state->default_gateway;
      bypass.interface = state->default_interface;
      if (!route_manager->add_route(bypass, ec)) {
        cli::print_warning("Failed to add server bypass route: " + ec.message());
        LOG_WARN("Failed to add server bypass route: {}", ec.message());
      }
    }

    // Add default route via tunnel
    if (!route_manager->add_default_route(tun.tun_device()->device_name(), "", 100, ec)) {
      cli::print_warning("Failed to set default route: " + ec.message());
      LOG_WARN("Failed to set default route: {}", ec.message());
    } else {
      cli::print_success("Default route configured");
    }
  }

#ifndef _WIN32
  // Setup signal handlers (POSIX only)
  auto& sig_handler = signal::SignalHandler::instance();
  sig_handler.on(signal::Signal::kInterrupt, [&tun](signal::Signal) {
    log_signal_sigint();
    tun.stop();
  });
  sig_handler.on(signal::Signal::kTerminate, [&tun](signal::Signal) {
    log_signal_sigterm();
    tun.stop();
  });
#endif

  // Run the tunnel
  std::cout << '\n';
  cli::print_section("Connection");
  cli::print_info("Connecting to " + config.tunnel.server_address + ":" +
                  std::to_string(config.tunnel.server_port) + "...");
  LOG_INFO("Starting tunnel to {}:{}", config.tunnel.server_address, config.tunnel.server_port);

  tun.run();

  // Cleanup routes
  cli::print_info("Cleaning up routes...");
  route_manager->cleanup();

  std::cout << '\n';
  cli::print_success("VEIL Client stopped gracefully");
  LOG_INFO("VEIL Client stopped");
  return EXIT_SUCCESS;
}
