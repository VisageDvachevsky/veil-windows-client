#ifdef _WIN32

// Windows Service entry point for VEIL VPN daemon
// This executable runs as a Windows service and manages the VPN connection.

#include <windows.h>

#include <atomic>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <thread>

#include "../client/client_config.h"
#include "../common/config/app_config.h"
#include "../common/ipc/ipc_protocol.h"
#include "../common/ipc/ipc_socket.h"
#include "../common/logging/logger.h"
#include "../tunnel/tunnel.h"
#include "service_manager.h"

using namespace veil;
using namespace veil::windows;

// Global state
static std::atomic<bool> g_running{false};
static std::unique_ptr<tunnel::Tunnel> g_tunnel;
static std::unique_ptr<ipc::IpcServer> g_ipc_server;

// Forward declarations
void WINAPI service_main(DWORD argc, LPSTR* argv);
void run_service();
void stop_service();
void handle_ipc_message(const ipc::Message& msg, int client_fd);

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
  // Check if running with console arguments for debugging/installation
  if (argc > 1) {
    std::string arg = argv[1];

    if (arg == "--install" || arg == "-i") {
      // Install the service
      if (!elevation::is_elevated()) {
        std::cout << "Administrator privileges required. Requesting elevation..."
                  << std::endl;
        return elevation::request_elevation("--install") ? 0 : 1;
      }

      // Get the path to this executable
      char path[MAX_PATH];
      GetModuleFileNameA(nullptr, path, MAX_PATH);

      std::string error;
      if (ServiceManager::install(path, error)) {
        std::cout << "Service installed successfully." << std::endl;
        return 0;
      } else {
        std::cerr << "Failed to install service: " << error << std::endl;
        return 1;
      }
    }

    if (arg == "--uninstall" || arg == "-u") {
      // Uninstall the service
      if (!elevation::is_elevated()) {
        std::cout << "Administrator privileges required. Requesting elevation..."
                  << std::endl;
        return elevation::request_elevation("--uninstall") ? 0 : 1;
      }

      std::string error;
      if (ServiceManager::uninstall(error)) {
        std::cout << "Service uninstalled successfully." << std::endl;
        return 0;
      } else {
        std::cerr << "Failed to uninstall service: " << error << std::endl;
        return 1;
      }
    }

    if (arg == "--start" || arg == "-s") {
      // Start the service
      std::string error;
      if (ServiceManager::start(error)) {
        std::cout << "Service started." << std::endl;
        return 0;
      } else {
        std::cerr << "Failed to start service: " << error << std::endl;
        return 1;
      }
    }

    if (arg == "--stop" || arg == "-t") {
      // Stop the service
      std::string error;
      if (ServiceManager::stop(error)) {
        std::cout << "Service stopped." << std::endl;
        return 0;
      } else {
        std::cerr << "Failed to stop service: " << error << std::endl;
        return 1;
      }
    }

    if (arg == "--status") {
      // Query service status
      if (!ServiceManager::is_installed()) {
        std::cout << "Service is not installed." << std::endl;
        return 1;
      }
      std::cout << "Service status: " << ServiceManager::get_status_string()
                << std::endl;
      return 0;
    }

    if (arg == "--debug" || arg == "-d") {
      // Run in console mode for debugging
      std::cout << "Running in debug mode (press Ctrl+C to stop)..."
                << std::endl;

      // Initialize logging to console
      logging::configure_logging(logging::LogLevel::kDebug, true);

      // Set up signal handler for Ctrl+C
      signal(SIGINT, [](int) {
        std::cout << "\nStopping..." << std::endl;
        stop_service();
      });

      run_service();
      return 0;
    }

    if (arg == "--help" || arg == "-h") {
      std::cout << "VEIL VPN Service\n"
                << "\n"
                << "Usage: veil-service.exe [options]\n"
                << "\n"
                << "Options:\n"
                << "  --install, -i    Install the Windows service\n"
                << "  --uninstall, -u  Uninstall the Windows service\n"
                << "  --start, -s      Start the service\n"
                << "  --stop, -t       Stop the service\n"
                << "  --status         Query service status\n"
                << "  --debug, -d      Run in console mode for debugging\n"
                << "  --help, -h       Show this help message\n"
                << std::endl;
      return 0;
    }

    std::cerr << "Unknown argument: " << arg << std::endl;
    std::cerr << "Use --help for usage information." << std::endl;
    return 1;
  }

  // No arguments - run as Windows service
  SERVICE_TABLE_ENTRYA service_table[] = {
      {const_cast<char*>(ServiceManager::kServiceName), service_main},
      {nullptr, nullptr}};

  if (!StartServiceCtrlDispatcherA(service_table)) {
    DWORD err = GetLastError();
    if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
      // Not being run as a service - show help
      std::cerr << "This program is intended to run as a Windows service.\n"
                << "Use --help for command line options." << std::endl;
    } else {
      std::cerr << "Failed to start service control dispatcher: " << err
                << std::endl;
    }
    return 1;
  }

  return 0;
}

// ============================================================================
// Service Main Function
// ============================================================================

void WINAPI service_main(DWORD /*argc*/, LPSTR* /*argv*/) {
  // Register service control handler
  if (!ServiceControlHandler::init(ServiceManager::kServiceName)) {
    return;
  }

  // Report starting
  ServiceControlHandler::report_starting(1);

  // Initialize logging to Windows Event Log
  logging::configure_logging(logging::LogLevel::kInfo, false);

  // Set stop handler
  ServiceControlHandler::on_stop([]() { stop_service(); });

  // Report starting progress
  ServiceControlHandler::report_starting(2);

  // Run the service
  run_service();

  // Report stopped
  ServiceControlHandler::report_stopped(NO_ERROR);
}

// ============================================================================
// Service Logic
// ============================================================================

void run_service() {
  g_running = true;

  // Load configuration
  std::filesystem::path config_path;

  // Try common config locations
  std::vector<std::filesystem::path> config_locations = {
      std::filesystem::path(getenv("PROGRAMDATA") ? getenv("PROGRAMDATA") : "") /
          "VEIL" / "client.json",
      std::filesystem::path(getenv("APPDATA") ? getenv("APPDATA") : "") /
          "VEIL" / "client.json",
      std::filesystem::current_path() / "client.json",
  };

  for (const auto& path : config_locations) {
    if (std::filesystem::exists(path)) {
      config_path = path;
      break;
    }
  }

  client::ClientConfig config;
  if (!config_path.empty()) {
    std::error_code ec;
    if (!client::load_config_file(config_path.string(), config, ec)) {
      LOG_WARN("Failed to load config from {}: {}", config_path.string(),
               ec.message());
    } else {
      LOG_INFO("Loaded configuration from {}", config_path.string());
    }
  }

  // Create IPC server for GUI communication
  g_ipc_server = std::make_unique<ipc::IpcServer>();
  g_ipc_server->on_message(handle_ipc_message);

  std::error_code ec;
  if (!g_ipc_server->start(ec)) {
    LOG_ERROR("Failed to start IPC server: {}", ec.message());
    // Continue without IPC - the tunnel can still work
  }

  // Create tunnel (but don't connect yet - wait for GUI command)
  g_tunnel = std::make_unique<tunnel::Tunnel>();

  // Report that we're running
  ServiceControlHandler::report_running();

  LOG_INFO("VEIL VPN Service started");

  // Main service loop
  while (g_running) {
    // Poll IPC server for messages
    if (g_ipc_server) {
      std::error_code ipc_ec;
      g_ipc_server->poll(ipc_ec);
    }

    // Poll tunnel if connected
    if (g_tunnel && g_tunnel->is_connected()) {
      std::error_code tunnel_ec;
      g_tunnel->poll(tunnel_ec);
    }

    // Small sleep to prevent busy-waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Cleanup
  if (g_tunnel) {
    g_tunnel->disconnect();
    g_tunnel.reset();
  }

  if (g_ipc_server) {
    g_ipc_server->stop();
    g_ipc_server.reset();
  }

  LOG_INFO("VEIL VPN Service stopped");
}

void stop_service() {
  g_running = false;
}

// ============================================================================
// IPC Message Handler
// ============================================================================

void handle_ipc_message(const ipc::Message& msg, int client_fd) {
  if (!std::holds_alternative<ipc::Command>(msg.payload)) {
    LOG_WARN("Received non-command message from client");
    return;
  }

  const auto& cmd = std::get<ipc::Command>(msg.payload);

  LOG_DEBUG("Received IPC command: {}", static_cast<int>(cmd.type));

  ipc::Response response;
  response.request_id = msg.request_id;

  switch (cmd.type) {
    case ipc::CommandType::kConnect: {
      if (g_tunnel && g_tunnel->is_connected()) {
        response.success = false;
        response.error_message = "Already connected";
      } else if (g_tunnel) {
        // Get connection parameters from command
        std::string server_host = "127.0.0.1";
        uint16_t server_port = 4433;

        auto it = cmd.parameters.find("host");
        if (it != cmd.parameters.end()) {
          server_host = it->second;
        }

        it = cmd.parameters.find("port");
        if (it != cmd.parameters.end()) {
          server_port = static_cast<uint16_t>(std::stoi(it->second));
        }

        std::error_code ec;
        if (g_tunnel->connect(server_host, server_port, ec)) {
          response.success = true;

          // Broadcast connection event
          ipc::Event event;
          event.type = ipc::EventType::kConnectionStateChanged;
          event.data["state"] = "connected";
          event.data["server"] = server_host + ":" + std::to_string(server_port);

          ipc::Message event_msg;
          event_msg.payload = event;
          g_ipc_server->broadcast_message(event_msg);
        } else {
          response.success = false;
          response.error_message = ec.message();
        }
      } else {
        response.success = false;
        response.error_message = "Tunnel not initialized";
      }
      break;
    }

    case ipc::CommandType::kDisconnect: {
      if (g_tunnel && g_tunnel->is_connected()) {
        g_tunnel->disconnect();
        response.success = true;

        // Broadcast disconnection event
        ipc::Event event;
        event.type = ipc::EventType::kConnectionStateChanged;
        event.data["state"] = "disconnected";

        ipc::Message event_msg;
        event_msg.payload = event;
        g_ipc_server->broadcast_message(event_msg);
      } else {
        response.success = false;
        response.error_message = "Not connected";
      }
      break;
    }

    case ipc::CommandType::kGetStatus: {
      response.success = true;
      if (g_tunnel) {
        response.data["connected"] =
            g_tunnel->is_connected() ? "true" : "false";
        // Add more status info as needed
      } else {
        response.data["connected"] = "false";
        response.data["error"] = "Tunnel not initialized";
      }
      break;
    }

    case ipc::CommandType::kGetStatistics: {
      response.success = true;
      if (g_tunnel) {
        const auto& stats = g_tunnel->stats();
        response.data["bytes_sent"] = std::to_string(stats.bytes_sent);
        response.data["bytes_received"] = std::to_string(stats.bytes_received);
        response.data["packets_sent"] = std::to_string(stats.packets_sent);
        response.data["packets_received"] = std::to_string(stats.packets_received);
      }
      break;
    }

    case ipc::CommandType::kSetConfig: {
      // Update configuration
      response.success = true;
      // TODO: Implement configuration updates
      break;
    }

    case ipc::CommandType::kGetConfig: {
      response.success = true;
      // TODO: Return current configuration
      break;
    }

    default:
      response.success = false;
      response.error_message = "Unknown command";
      break;
  }

  // Send response
  ipc::Message response_msg;
  response_msg.request_id = msg.request_id;
  response_msg.payload = response;

  std::error_code ec;
  g_ipc_server->send_message(client_fd, response_msg, ec);
}

#else
// Non-Windows builds - provide stub main
int main() {
  // This file is Windows-only
  return 1;
}
#endif  // _WIN32
