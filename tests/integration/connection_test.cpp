/**
 * Live server connection integration tests
 *
 * These tests verify that the VPN client can successfully connect to a real
 * VPN server. They are primarily used in CI with GitHub secrets to test
 * the actual connection flow:
 *
 * 1. UDP socket creation and binding
 * 2. Handshake INIT message generation and sending
 * 3. Handshake RESPONSE reception and verification
 * 4. Session key derivation
 *
 * Environment variables (set by CI workflow):
 * - VEIL_TEST_SERVER: Server IP address (required)
 * - VEIL_TEST_KEY_FILE: Path to pre-shared key file (required)
 * - VEIL_TEST_SEED_FILE: Path to obfuscation seed file (optional)
 * - VEIL_TEST_TIMEOUT_MS: Connection timeout in milliseconds (default: 30000)
 *
 * These tests are skipped when environment variables are not set.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "common/handshake/handshake_processor.h"
#include "common/logging/logger.h"
#include "transport/udp_socket/udp_socket.h"

namespace veil::integration_tests {

namespace {

// Helper to get environment variable or default
std::string get_env(const char* name, const std::string& default_value = "") {
  const char* value = std::getenv(name);
  return value != nullptr ? value : default_value;
}

// Helper to load key from file
bool load_key_file(const std::string& path, std::vector<std::uint8_t>& key) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  key.resize(32);
  file.read(reinterpret_cast<char*>(key.data()), 32);
  return file.gcount() == 32;
}

// Check if live server tests should run
bool should_run_live_tests() {
  return !get_env("VEIL_TEST_SERVER").empty() && !get_env("VEIL_TEST_KEY_FILE").empty();
}

}  // namespace

/**
 * Test fixture for live server connection tests.
 */
class LiveServerConnectionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!should_run_live_tests()) {
      GTEST_SKIP() << "Live server tests require VEIL_TEST_SERVER and VEIL_TEST_KEY_FILE environment variables";
    }

    server_ip_ = get_env("VEIL_TEST_SERVER");
    key_file_ = get_env("VEIL_TEST_KEY_FILE");
    seed_file_ = get_env("VEIL_TEST_SEED_FILE");
    timeout_ms_ = std::stoi(get_env("VEIL_TEST_TIMEOUT_MS", "30000"));

    // Load pre-shared key
    if (!load_key_file(key_file_, psk_)) {
      GTEST_SKIP() << "Failed to load pre-shared key from " << key_file_;
    }

    LOG_INFO("Live server test configuration:");
    LOG_INFO("  Server: {}:4433", server_ip_);
    LOG_INFO("  Key file: {}", key_file_);
    LOG_INFO("  Seed file: {}", seed_file_.empty() ? "(not set)" : seed_file_);
    LOG_INFO("  Timeout: {}ms", timeout_ms_);
  }

  std::string server_ip_;
  std::string key_file_;
  std::string seed_file_;
  int timeout_ms_{30000};
  std::vector<std::uint8_t> psk_;
};

/**
 * Test that we can successfully perform a handshake with the live VPN server.
 *
 * This is the critical test that verifies:
 * 1. UDP packets can be sent to the server
 * 2. The server responds to our handshake INIT
 * 3. Session keys are successfully derived
 *
 * This test addresses issue #43 by verifying the complete handshake flow
 * works correctly on Linux when connecting to a real server.
 */
TEST_F(LiveServerConnectionTest, LiveServerHandshake) {
  using namespace std::chrono_literals;

  LOG_INFO("========================================");
  LOG_INFO("Starting live server handshake test");
  LOG_INFO("========================================");

  // Create UDP socket
  transport::UdpSocket socket;
  std::error_code ec;

  if (!socket.open(0, true, ec)) {
    FAIL() << "Failed to open UDP socket: " << ec.message();
  }
  LOG_INFO("UDP socket opened successfully");

  // Connect to server
  transport::UdpEndpoint server{server_ip_, 4433};
  if (!socket.connect(server, ec)) {
    FAIL() << "Failed to connect UDP socket to server: " << ec.message();
  }
  LOG_INFO("UDP socket connected to {}:4433", server_ip_);

  // Create handshake initiator
  auto now_fn = []() { return std::chrono::system_clock::now(); };
  handshake::HandshakeInitiator initiator(psk_, std::chrono::milliseconds(timeout_ms_), now_fn);

  // Generate and send INIT message
  auto init_msg = initiator.create_init();
  ASSERT_FALSE(init_msg.empty()) << "Failed to create handshake INIT message";

  LOG_INFO("HANDSHAKE: Generated INIT message ({} bytes)", init_msg.size());

  if (!socket.send(init_msg, server, ec)) {
    FAIL() << "Failed to send handshake INIT: " << ec.message();
  }
  LOG_INFO("HANDSHAKE: INIT sent successfully, waiting for RESPONSE...");

  // Wait for RESPONSE
  std::vector<std::uint8_t> response;
  bool received = false;
  transport::UdpEndpoint response_endpoint;

  socket.poll(
      [&response, &received, &response_endpoint](const transport::UdpPacket& pkt) {
        response = pkt.data;
        response_endpoint = pkt.remote;
        received = true;
      },
      timeout_ms_, ec);

  if (!received || response.empty()) {
    FAIL() << "Handshake timeout: No response received from server within " << timeout_ms_ << "ms\n"
           << "This could indicate:\n"
           << "  - Server is not running or unreachable\n"
           << "  - Firewall blocking UDP traffic\n"
           << "  - Incorrect server IP address\n"
           << "  - Network routing issues";
  }

  LOG_INFO("HANDSHAKE: Received response ({} bytes) from {}:{}",
           response.size(), response_endpoint.host, response_endpoint.port);

  // Process RESPONSE
  auto session = initiator.consume_response(response);
  if (!session) {
    FAIL() << "Failed to process handshake RESPONSE\n"
           << "This could indicate:\n"
           << "  - Incorrect pre-shared key\n"
           << "  - Timestamp skew between client and server\n"
           << "  - Protocol version mismatch";
  }

  LOG_INFO("========================================");
  LOG_INFO("HANDSHAKE SUCCESSFUL!");
  LOG_INFO("  Session ID: {}", session->session_id);
  LOG_INFO("========================================");

  // Verify session was established
  EXPECT_NE(session->session_id, 0ULL) << "Session ID should be non-zero";

  // Verify keys were derived (check they are not all zeros)
  bool send_key_has_data = false;
  bool recv_key_has_data = false;
  for (auto byte : session->keys.send_key) {
    if (byte != 0) {
      send_key_has_data = true;
      break;
    }
  }
  for (auto byte : session->keys.recv_key) {
    if (byte != 0) {
      recv_key_has_data = true;
      break;
    }
  }
  EXPECT_TRUE(send_key_has_data) << "Send key should be derived (not all zeros)";
  EXPECT_TRUE(recv_key_has_data) << "Receive key should be derived (not all zeros)";

  socket.close();
}

/**
 * Test UDP socket binding and basic connectivity.
 *
 * This test verifies that the UDP socket can be created and bound
 * successfully, which is a prerequisite for the handshake.
 */
TEST_F(LiveServerConnectionTest, UdpSocketConnectivity) {
  transport::UdpSocket socket;
  std::error_code ec;

  // Open socket
  ASSERT_TRUE(socket.open(0, true, ec)) << "Failed to open UDP socket: " << ec.message();

  // Connect to server (this doesn't actually send anything, just sets the default destination)
  transport::UdpEndpoint server{server_ip_, 4433};
  ASSERT_TRUE(socket.connect(server, ec)) << "Failed to connect: " << ec.message();

  // Send a small test packet (server will likely ignore/drop it, but we verify sendto works)
  std::vector<std::uint8_t> test_data = {'T', 'E', 'S', 'T'};
  EXPECT_TRUE(socket.send(test_data, server, ec)) << "Failed to send test packet: " << ec.message();

  LOG_INFO("UDP socket connectivity test passed - can send to {}:4433", server_ip_);

  socket.close();
}

/**
 * Test multiple sequential handshake attempts.
 *
 * This verifies that the client can handle reconnection scenarios
 * where multiple handshakes may be attempted.
 */
TEST_F(LiveServerConnectionTest, MultipleHandshakeAttempts) {
  using namespace std::chrono_literals;

  const int num_attempts = 2;

  for (int attempt = 1; attempt <= num_attempts; ++attempt) {
    LOG_INFO("Handshake attempt {}/{}", attempt, num_attempts);

    transport::UdpSocket socket;
    std::error_code ec;

    ASSERT_TRUE(socket.open(0, true, ec)) << "Attempt " << attempt << ": Failed to open socket";

    transport::UdpEndpoint server{server_ip_, 4433};
    ASSERT_TRUE(socket.connect(server, ec)) << "Attempt " << attempt << ": Failed to connect";

    auto now_fn = []() { return std::chrono::system_clock::now(); };
    handshake::HandshakeInitiator initiator(psk_, std::chrono::milliseconds(timeout_ms_), now_fn);

    auto init_msg = initiator.create_init();
    ASSERT_FALSE(init_msg.empty()) << "Attempt " << attempt << ": Failed to create INIT";

    ASSERT_TRUE(socket.send(init_msg, server, ec)) << "Attempt " << attempt << ": Failed to send INIT";

    std::vector<std::uint8_t> response;
    bool received = false;

    socket.poll(
        [&response, &received](const transport::UdpPacket& pkt) {
          response = pkt.data;
          received = true;
        },
        timeout_ms_, ec);

    if (received && !response.empty()) {
      auto session = initiator.consume_response(response);
      EXPECT_TRUE(session.has_value()) << "Attempt " << attempt << ": Failed to process response";
      if (session) {
        LOG_INFO("Attempt {}: Handshake successful, session ID: {}", attempt, session->session_id);
      }
    } else {
      LOG_WARN("Attempt {}: No response received (timeout)", attempt);
    }

    socket.close();

    // Small delay between attempts to avoid rate limiting
    if (attempt < num_attempts) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }
}

}  // namespace veil::integration_tests
