#include "common/protocol_wrapper/tls_wrapper.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace veil::protocol_wrapper {

namespace {

// Write big-endian uint16.
void write_be_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

// Read big-endian uint16.
std::uint16_t read_be_u16(std::span<const std::uint8_t> data, std::size_t offset) {
  return static_cast<std::uint16_t>((data[offset] << 8) | data[offset + 1]);
}

}  // namespace

std::vector<std::uint8_t> TLSWrapper::wrap(std::span<const std::uint8_t> data) {
  std::vector<std::uint8_t> result;

  // Reserve approximate space: header(5) per record + payload.
  const std::size_t num_records =
      data.empty() ? 1 : (data.size() + kMaxTLSRecordPayload - 1) / kMaxTLSRecordPayload;
  result.reserve(num_records * kTLSRecordHeaderSize + data.size());

  std::size_t offset = 0;
  do {
    const std::size_t chunk_size =
        std::min(static_cast<std::size_t>(kMaxTLSRecordPayload), data.size() - offset);

    TLSRecordHeader header;
    header.content_type = TLSContentType::kApplicationData;
    header.legacy_version = 0x0303;
    header.length = static_cast<std::uint16_t>(chunk_size);

    auto header_bytes = build_header(header);
    result.insert(result.end(), header_bytes.begin(), header_bytes.end());
    result.insert(result.end(), data.begin() + static_cast<std::ptrdiff_t>(offset),
                  data.begin() + static_cast<std::ptrdiff_t>(offset + chunk_size));

    offset += chunk_size;
  } while (offset < data.size());

  return result;
}

std::optional<std::vector<std::uint8_t>> TLSWrapper::unwrap(
    std::span<const std::uint8_t> record) {
  auto header_result = parse_header(record);
  if (!header_result.has_value()) {
    return std::nullopt;
  }

  const auto& [header, payload_offset] = *header_result;

  // Verify content type is application data.
  if (header.content_type != TLSContentType::kApplicationData) {
    return std::nullopt;
  }

  // Check that record has complete payload.
  if (record.size() < payload_offset + header.length) {
    return std::nullopt;
  }

  // Extract payload.
  return std::vector<std::uint8_t>(
      record.begin() + static_cast<std::ptrdiff_t>(payload_offset),
      record.begin() + static_cast<std::ptrdiff_t>(payload_offset + header.length));
}

std::optional<std::vector<std::uint8_t>> TLSWrapper::unwrap_all(
    std::span<const std::uint8_t> data) {
  std::vector<std::uint8_t> result;
  std::size_t offset = 0;

  while (offset < data.size()) {
    auto remaining = data.subspan(offset);
    auto header_result = parse_header(remaining);
    if (!header_result.has_value()) {
      return std::nullopt;
    }

    const auto& [header, payload_offset] = *header_result;

    if (header.content_type != TLSContentType::kApplicationData) {
      return std::nullopt;
    }

    if (remaining.size() < payload_offset + header.length) {
      return std::nullopt;
    }

    result.insert(result.end(),
                  remaining.begin() + static_cast<std::ptrdiff_t>(payload_offset),
                  remaining.begin() + static_cast<std::ptrdiff_t>(payload_offset + header.length));

    offset += payload_offset + header.length;
  }

  return result;
}

std::optional<std::pair<TLSRecordHeader, std::size_t>> TLSWrapper::parse_header(
    std::span<const std::uint8_t> data) {
  // TLS record header is exactly 5 bytes.
  if (data.size() < kTLSRecordHeaderSize) {
    return std::nullopt;
  }

  TLSRecordHeader header;
  header.content_type = static_cast<TLSContentType>(data[0]);
  header.legacy_version = read_be_u16(data, 1);
  header.length = read_be_u16(data, 3);

  // Validate content type is a known TLS content type.
  switch (header.content_type) {
    case TLSContentType::kChangeCipherSpec:
    case TLSContentType::kAlert:
    case TLSContentType::kHandshake:
    case TLSContentType::kApplicationData:
      break;
    default:
      return std::nullopt;
  }

  // Validate length does not exceed maximum TLS record size.
  if (header.length > kMaxTLSRecordPayload) {
    return std::nullopt;
  }

  return std::make_pair(header, kTLSRecordHeaderSize);
}

std::vector<std::uint8_t> TLSWrapper::build_header(const TLSRecordHeader& header) {
  std::vector<std::uint8_t> result;
  result.reserve(kTLSRecordHeaderSize);

  result.push_back(static_cast<std::uint8_t>(header.content_type));
  write_be_u16(result, header.legacy_version);
  write_be_u16(result, header.length);

  return result;
}

}  // namespace veil::protocol_wrapper
