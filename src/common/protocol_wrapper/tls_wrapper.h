#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace veil::protocol_wrapper {

// TLS content types (RFC 8446 Section 5.1).
enum class TLSContentType : std::uint8_t {
  kChangeCipherSpec = 0x14,
  kAlert = 0x15,
  kHandshake = 0x16,
  kApplicationData = 0x17,
};

// TLS record header (RFC 8446 Section 5.1).
// TLS 1.3 uses legacy_version 0x0303 (TLS 1.2) for compatibility.
//
// Record format:
//   Byte 0:   Content type (0x17 = application_data)
//   Bytes 1-2: Legacy version (0x0303 = TLS 1.2)
//   Bytes 3-4: Payload length (big-endian uint16)
//   Bytes 5+:  Payload data
//
// Overhead: 5 bytes per record (fixed header size).
struct TLSRecordHeader {
  TLSContentType content_type{TLSContentType::kApplicationData};
  std::uint16_t legacy_version{0x0303};  // TLS 1.2 for compatibility (RFC 8446)
  std::uint16_t length{0};               // Payload length (max 16384 = 2^14)
};

// Maximum TLS record payload length (RFC 8446 Section 5.1).
constexpr std::uint16_t kMaxTLSRecordPayload = 16384;

// TLS record header size in bytes.
constexpr std::size_t kTLSRecordHeaderSize = 5;

// TLS record layer wrapper for DPI evasion.
// Wraps VEIL packets in TLS 1.3 application data records to mimic legitimate
// wss:// (WebSocket over TLS) traffic.
//
// This is a cosmetic wrapper only — it does NOT perform actual TLS encryption.
// The wrapped data already uses ChaCha20-Poly1305 AEAD encryption at a lower
// layer. The TLS record header makes the traffic appear as legitimate TLS
// application data to DPI systems.
//
// Usage:
//   auto wrapped = TLSWrapper::wrap(veil_packet);
//   auto unwrapped = TLSWrapper::unwrap(wrapped);
//
// TLS 1.3 Record format (RFC 8446 Section 5.1):
//   +--------+--------+--------+--------+--------+
//   | Type   | Legacy version  | Length           |
//   | (0x17) | (0x03) | (0x03) | (MSB)  | (LSB)  |
//   +--------+--------+--------+--------+--------+
//   |                Payload data ...              |
//   +----------------------------------------------+
class TLSWrapper {
 public:
  // Wrap data in a TLS application data record.
  // For payloads exceeding kMaxTLSRecordPayload (16384 bytes), the data is
  // split into multiple TLS records concatenated together.
  static std::vector<std::uint8_t> wrap(std::span<const std::uint8_t> data);

  // Unwrap a TLS application data record and return the payload.
  // Returns std::nullopt if the record is invalid or incomplete.
  // If multiple records are concatenated, only the first record is unwrapped.
  static std::optional<std::vector<std::uint8_t>> unwrap(
      std::span<const std::uint8_t> record);

  // Unwrap all concatenated TLS records and return the combined payload.
  // Returns std::nullopt if any record is invalid or incomplete.
  static std::optional<std::vector<std::uint8_t>> unwrap_all(
      std::span<const std::uint8_t> data);

  // Parse TLS record header.
  // Returns header and the offset where payload starts, or std::nullopt if invalid.
  static std::optional<std::pair<TLSRecordHeader, std::size_t>> parse_header(
      std::span<const std::uint8_t> data);

  // Build TLS record header bytes.
  static std::vector<std::uint8_t> build_header(const TLSRecordHeader& header);
};

}  // namespace veil::protocol_wrapper
