// Experiment to verify cross-platform crypto compatibility for Issue #72
// This test verifies that:
// 1. Key derivation produces consistent results
// 2. Sequence obfuscation is reversible
// 3. AEAD encryption/decryption works correctly
// 4. No endianness issues in sequence encoding

#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <sodium.h>

#include "common/crypto/crypto_engine.h"
#include "common/crypto/random.h"
#include "common/handshake/handshake_processor.h"
#include "transport/session/transport_session.h"

using namespace veil;

void fill_random(std::span<std::uint8_t> buffer) {
    randombytes_buf(buffer.data(), buffer.size());
}

void print_hex(const char* label, const std::uint8_t* data, std::size_t len) {
    std::cout << label << ": ";
    for (std::size_t i = 0; i < len && i < 32; ++i) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]);
    }
    if (len > 32) std::cout << "...";
    std::cout << std::dec << " (len=" << len << ")\n";
}

void print_fingerprint(const char* label, const std::uint8_t* data) {
    std::cout << label << ": " << std::hex;
    for (int i = 0; i < 4; ++i) {
        std::cout << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]);
    }
    std::cout << std::dec << "\n";
}

bool test_sequence_obfuscation() {
    std::cout << "\n=== Test 1: Sequence Obfuscation ===\n";

    // Generate a random obfuscation key
    std::array<std::uint8_t, crypto::kAeadKeyLen> key;
    fill_random(key);

    print_hex("Obfuscation key", key.data(), key.size());

    // Test various sequence numbers
    std::vector<std::uint64_t> test_sequences = {0, 1, 255, 256, 65535, 65536,
                                                  0x7FFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF};

    for (auto seq : test_sequences) {
        auto obfuscated = crypto::obfuscate_sequence(seq, key);
        auto deobfuscated = crypto::deobfuscate_sequence(obfuscated, key);

        std::cout << "  seq=" << seq << " -> obfuscated=" << std::hex << obfuscated
                  << " -> deobfuscated=" << std::dec << deobfuscated;

        if (deobfuscated != seq) {
            std::cout << " FAILED!\n";
            return false;
        }
        std::cout << " OK\n";
    }

    return true;
}

bool test_sequence_encoding_big_endian() {
    std::cout << "\n=== Test 2: Sequence Encoding (Big Endian) ===\n";

    std::uint64_t test_seq = 0x0102030405060708ULL;

    // Encode to big-endian (same as build_encrypted_packet)
    std::vector<std::uint8_t> encoded(8);
    for (int i = 7; i >= 0; --i) {
        encoded[7 - i] = static_cast<std::uint8_t>((test_seq >> (8 * i)) & 0xFF);
    }

    // Decode from big-endian (same as decrypt_packet)
    std::uint64_t decoded = 0;
    for (int i = 0; i < 8; ++i) {
        decoded = (decoded << 8) | encoded[static_cast<std::size_t>(i)];
    }

    std::cout << "  Original:  " << std::hex << test_seq << std::dec << "\n";
    std::cout << "  Encoded:   ";
    for (auto b : encoded) std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
    std::cout << std::dec << "\n";
    std::cout << "  Decoded:   " << std::hex << decoded << std::dec << "\n";

    if (decoded != test_seq) {
        std::cout << "  FAILED!\n";
        return false;
    }
    std::cout << "  OK\n";
    return true;
}

bool test_aead_roundtrip() {
    std::cout << "\n=== Test 3: AEAD Encryption/Decryption Roundtrip ===\n";

    // Generate random key and nonce
    std::array<std::uint8_t, crypto::kAeadKeyLen> key;
    std::array<std::uint8_t, crypto::kNonceLen> nonce;
    fill_random(key);
    fill_random(nonce);

    // Test data
    std::vector<std::uint8_t> plaintext = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20, 0x57, 0x6f, 0x72, 0x6c, 0x64, 0x21};

    print_hex("Key", key.data(), key.size());
    print_hex("Nonce", nonce.data(), nonce.size());
    print_hex("Plaintext", plaintext.data(), plaintext.size());

    // Encrypt
    auto ciphertext = crypto::aead_encrypt(key, nonce, {}, plaintext);
    print_hex("Ciphertext", ciphertext.data(), ciphertext.size());

    // Decrypt
    auto decrypted = crypto::aead_decrypt(key, nonce, {}, ciphertext);
    if (!decrypted) {
        std::cout << "  Decryption FAILED!\n";
        return false;
    }

    print_hex("Decrypted", decrypted->data(), decrypted->size());

    if (*decrypted != plaintext) {
        std::cout << "  Mismatch!\n";
        return false;
    }

    std::cout << "  OK\n";
    return true;
}

bool test_key_derivation_symmetry() {
    std::cout << "\n=== Test 4: Key Derivation Symmetry (Initiator vs Responder) ===\n";

    // Simulate shared secret (from X25519) - use X25519SecretKeySize which is 32
    std::array<std::uint8_t, 32> shared_secret;
    fill_random(shared_secret);

    // Simulate PSK - also 32 bytes
    std::array<std::uint8_t, 32> psk;
    fill_random(psk);

    // Info string
    std::string info = "test-session-info";

    // Derive keys for initiator
    auto initiator_keys = crypto::derive_session_keys(shared_secret, psk,
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(info.data()), info.size()), true);

    // Derive keys for responder
    auto responder_keys = crypto::derive_session_keys(shared_secret, psk,
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(info.data()), info.size()), false);

    std::cout << "Initiator:\n";
    print_fingerprint("  send_key", initiator_keys.send_key.data());
    print_fingerprint("  recv_key", initiator_keys.recv_key.data());
    print_fingerprint("  send_nonce", initiator_keys.send_nonce.data());
    print_fingerprint("  recv_nonce", initiator_keys.recv_nonce.data());

    std::cout << "Responder:\n";
    print_fingerprint("  send_key", responder_keys.send_key.data());
    print_fingerprint("  recv_key", responder_keys.recv_key.data());
    print_fingerprint("  send_nonce", responder_keys.send_nonce.data());
    print_fingerprint("  recv_nonce", responder_keys.recv_nonce.data());

    // Verify symmetry: initiator's send = responder's recv
    bool ok = true;
    if (initiator_keys.send_key != responder_keys.recv_key) {
        std::cout << "  FAILED: initiator.send_key != responder.recv_key\n";
        ok = false;
    }
    if (initiator_keys.recv_key != responder_keys.send_key) {
        std::cout << "  FAILED: initiator.recv_key != responder.send_key\n";
        ok = false;
    }
    if (initiator_keys.send_nonce != responder_keys.recv_nonce) {
        std::cout << "  FAILED: initiator.send_nonce != responder.recv_nonce\n";
        ok = false;
    }
    if (initiator_keys.recv_nonce != responder_keys.send_nonce) {
        std::cout << "  FAILED: initiator.recv_nonce != responder.send_nonce\n";
        ok = false;
    }

    if (ok) std::cout << "  All symmetry checks OK\n";
    return ok;
}

bool test_full_packet_roundtrip() {
    std::cout << "\n=== Test 5: Full Packet Roundtrip (Simulating Client -> Server) ===\n";

    // Create a mock handshake session
    handshake::HandshakeSession hs_session;
    hs_session.session_id = 12345678901234567890ULL;

    // Generate key material
    std::array<std::uint8_t, 32> shared_secret;
    std::array<std::uint8_t, 32> psk;
    fill_random(shared_secret);
    fill_random(psk);

    std::string info = "test-session";

    // Derive keys for initiator (client)
    auto client_keys = crypto::derive_session_keys(shared_secret, psk,
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(info.data()), info.size()), true);

    // Derive keys for responder (server)
    auto server_keys = crypto::derive_session_keys(shared_secret, psk,
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(info.data()), info.size()), false);

    // Create transport sessions
    hs_session.keys = client_keys;
    transport::TransportSessionConfig config;
    auto client_session = std::make_unique<transport::TransportSession>(hs_session, config);

    hs_session.keys = server_keys;
    auto server_session = std::make_unique<transport::TransportSession>(hs_session, config);

    // Client encrypts data
    std::vector<std::uint8_t> plaintext = {'H', 'e', 'l', 'l', 'o', ' ', 'f', 'r', 'o', 'm', ' ', 'c', 'l', 'i', 'e', 'n', 't'};
    print_hex("Client plaintext", plaintext.data(), plaintext.size());

    auto packets = client_session->encrypt_data(plaintext);
    std::cout << "  Client produced " << packets.size() << " packet(s)\n";

    for (std::size_t i = 0; i < packets.size(); ++i) {
        print_hex(("  Packet " + std::to_string(i)).c_str(), packets[i].data(), packets[i].size());
    }

    // Server decrypts data
    for (const auto& pkt : packets) {
        auto frames = server_session->decrypt_packet(pkt);
        if (!frames) {
            std::cout << "  SERVER DECRYPTION FAILED!\n";
            return false;
        }
        std::cout << "  Server decrypted " << frames->size() << " frame(s)\n";
        for (const auto& frame : *frames) {
            if (frame.kind == mux::FrameKind::kData) {
                print_hex("  Decrypted payload", frame.data.payload.data(), frame.data.payload.size());
            }
        }
    }

    std::cout << "  OK\n";
    return true;
}

int main() {
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium\n";
        return 1;
    }

    std::cout << "Cross-Platform Crypto Compatibility Test for Issue #72\n";
    std::cout << "=====================================================\n";

    bool all_passed = true;

    all_passed &= test_sequence_obfuscation();
    all_passed &= test_sequence_encoding_big_endian();
    all_passed &= test_aead_roundtrip();
    all_passed &= test_key_derivation_symmetry();
    all_passed &= test_full_packet_roundtrip();

    std::cout << "\n=====================================================\n";
    if (all_passed) {
        std::cout << "All tests PASSED\n";
        return 0;
    } else {
        std::cout << "Some tests FAILED\n";
        return 1;
    }
}
