#include "common/protocol_wrapper/tls_wrapper.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using namespace veil::protocol_wrapper;

// Test basic wrap and unwrap.
TEST(TLSWrapperTest, WrapUnwrap) {
  std::vector<std::uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05};

  auto wrapped = TLSWrapper::wrap(payload);

  // Wrapped should be 5-byte header + payload.
  EXPECT_EQ(wrapped.size(), kTLSRecordHeaderSize + payload.size());

  auto unwrapped = TLSWrapper::unwrap(wrapped);
  ASSERT_TRUE(unwrapped.has_value());
  EXPECT_EQ(*unwrapped, payload);
}

// Test empty payload.
TEST(TLSWrapperTest, EmptyPayload) {
  std::vector<std::uint8_t> payload;

  auto wrapped = TLSWrapper::wrap(payload);

  // Should have just the 5-byte header.
  EXPECT_EQ(wrapped.size(), kTLSRecordHeaderSize);

  auto unwrapped = TLSWrapper::unwrap(wrapped);
  ASSERT_TRUE(unwrapped.has_value());
  EXPECT_EQ(unwrapped->size(), 0);
}

// Test small payload.
TEST(TLSWrapperTest, SmallPayload) {
  std::vector<std::uint8_t> payload(100, 0x42);

  auto wrapped = TLSWrapper::wrap(payload);
  EXPECT_EQ(wrapped.size(), kTLSRecordHeaderSize + 100);

  auto unwrapped = TLSWrapper::unwrap(wrapped);
  ASSERT_TRUE(unwrapped.has_value());
  EXPECT_EQ(*unwrapped, payload);
}

// Test medium payload (typical VEIL packet).
TEST(TLSWrapperTest, MediumPayload) {
  std::vector<std::uint8_t> payload(1500, 0x99);

  auto wrapped = TLSWrapper::wrap(payload);
  auto unwrapped = TLSWrapper::unwrap(wrapped);

  ASSERT_TRUE(unwrapped.has_value());
  EXPECT_EQ(*unwrapped, payload);
}

// Test large payload that requires multiple records.
TEST(TLSWrapperTest, LargePayloadMultipleRecords) {
  // 30000 bytes > 16384 max, so should produce 2 records.
  std::vector<std::uint8_t> payload(30000, 0x7F);

  auto wrapped = TLSWrapper::wrap(payload);

  // Should be 2 records: 16384 + 13616 = 30000 bytes payload.
  // Wrapped size = 2 * 5 (headers) + 30000 (payload).
  EXPECT_EQ(wrapped.size(), 2 * kTLSRecordHeaderSize + payload.size());

  // unwrap should only get first record.
  auto first_record = TLSWrapper::unwrap(wrapped);
  ASSERT_TRUE(first_record.has_value());
  EXPECT_EQ(first_record->size(), kMaxTLSRecordPayload);

  // unwrap_all should get entire payload.
  auto all_data = TLSWrapper::unwrap_all(wrapped);
  ASSERT_TRUE(all_data.has_value());
  EXPECT_EQ(*all_data, payload);
}

// Test exactly max record size payload.
TEST(TLSWrapperTest, ExactMaxPayload) {
  std::vector<std::uint8_t> payload(kMaxTLSRecordPayload, 0xAB);

  auto wrapped = TLSWrapper::wrap(payload);

  // Single record.
  EXPECT_EQ(wrapped.size(), kTLSRecordHeaderSize + kMaxTLSRecordPayload);

  auto unwrapped = TLSWrapper::unwrap(wrapped);
  ASSERT_TRUE(unwrapped.has_value());
  EXPECT_EQ(*unwrapped, payload);
}

// Test one byte over max record size.
TEST(TLSWrapperTest, OneOverMaxPayload) {
  std::vector<std::uint8_t> payload(kMaxTLSRecordPayload + 1, 0xCD);

  auto wrapped = TLSWrapper::wrap(payload);

  // Should produce 2 records: one full (16384) + one with 1 byte.
  EXPECT_EQ(wrapped.size(), 2 * kTLSRecordHeaderSize + payload.size());

  auto all_data = TLSWrapper::unwrap_all(wrapped);
  ASSERT_TRUE(all_data.has_value());
  EXPECT_EQ(*all_data, payload);
}

// Test parse_header.
TEST(TLSWrapperTest, ParseHeader) {
  std::vector<std::uint8_t> payload = {0x01, 0x02, 0x03};
  auto wrapped = TLSWrapper::wrap(payload);

  auto header_result = TLSWrapper::parse_header(wrapped);
  ASSERT_TRUE(header_result.has_value());

  const auto& [header, offset] = *header_result;

  EXPECT_EQ(header.content_type, TLSContentType::kApplicationData);
  EXPECT_EQ(header.legacy_version, 0x0303);
  EXPECT_EQ(header.length, 3);
  EXPECT_EQ(offset, kTLSRecordHeaderSize);
}

// Test parse_header with manually constructed TLS record.
TEST(TLSWrapperTest, ParseHeaderManual) {
  // Manually construct a TLS application data record header.
  std::vector<std::uint8_t> data = {
      0x17,        // Content type: application_data
      0x03, 0x03,  // Legacy version: TLS 1.2
      0x00, 0x0A,  // Length: 10 bytes
      // 10 bytes of payload follow...
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};

  auto header_result = TLSWrapper::parse_header(data);
  ASSERT_TRUE(header_result.has_value());

  const auto& [header, offset] = *header_result;

  EXPECT_EQ(header.content_type, TLSContentType::kApplicationData);
  EXPECT_EQ(header.legacy_version, 0x0303);
  EXPECT_EQ(header.length, 10);
  EXPECT_EQ(offset, 5);
}

// Test build_header.
TEST(TLSWrapperTest, BuildHeader) {
  TLSRecordHeader header;
  header.content_type = TLSContentType::kApplicationData;
  header.legacy_version = 0x0303;
  header.length = 256;

  auto header_bytes = TLSWrapper::build_header(header);

  EXPECT_EQ(header_bytes.size(), kTLSRecordHeaderSize);
  EXPECT_EQ(header_bytes[0], 0x17);  // application_data
  EXPECT_EQ(header_bytes[1], 0x03);  // Version MSB
  EXPECT_EQ(header_bytes[2], 0x03);  // Version LSB
  EXPECT_EQ(header_bytes[3], 0x01);  // Length MSB (256 >> 8)
  EXPECT_EQ(header_bytes[4], 0x00);  // Length LSB (256 & 0xFF)
}

// Test build_header with zero length.
TEST(TLSWrapperTest, BuildHeaderZeroLength) {
  TLSRecordHeader header;
  header.content_type = TLSContentType::kApplicationData;
  header.legacy_version = 0x0303;
  header.length = 0;

  auto header_bytes = TLSWrapper::build_header(header);

  EXPECT_EQ(header_bytes.size(), kTLSRecordHeaderSize);
  EXPECT_EQ(header_bytes[0], 0x17);
  EXPECT_EQ(header_bytes[3], 0x00);
  EXPECT_EQ(header_bytes[4], 0x00);
}

// Test invalid record (too short).
TEST(TLSWrapperTest, UnwrapInvalidRecordTooShort) {
  std::vector<std::uint8_t> invalid = {0x17, 0x03, 0x03};  // Only 3 bytes

  auto unwrapped = TLSWrapper::unwrap(invalid);
  EXPECT_FALSE(unwrapped.has_value());
}

// Test invalid record (incomplete payload).
TEST(TLSWrapperTest, UnwrapInvalidRecordIncompletePayload) {
  std::vector<std::uint8_t> invalid = {
      0x17,        // application_data
      0x03, 0x03,  // TLS 1.2
      0x00, 0x05,  // Length: 5
      0x01, 0x02   // Only 2 bytes of payload (expected 5)
  };

  auto unwrapped = TLSWrapper::unwrap(invalid);
  EXPECT_FALSE(unwrapped.has_value());
}

// Test invalid content type.
TEST(TLSWrapperTest, UnwrapInvalidContentType) {
  std::vector<std::uint8_t> invalid = {
      0x00,        // Invalid content type
      0x03, 0x03,  // TLS 1.2
      0x00, 0x01,  // Length: 1
      0xAA         // Payload
  };

  auto unwrapped = TLSWrapper::unwrap(invalid);
  EXPECT_FALSE(unwrapped.has_value());
}

// Test parse_header rejects oversized length.
TEST(TLSWrapperTest, ParseHeaderRejectsOversizedLength) {
  std::vector<std::uint8_t> data = {
      0x17,        // application_data
      0x03, 0x03,  // TLS 1.2
      0x40, 0x01,  // Length: 16385 (exceeds max 16384)
  };

  auto header_result = TLSWrapper::parse_header(data);
  EXPECT_FALSE(header_result.has_value());
}

// Test unwrap_all with invalid concatenated records.
TEST(TLSWrapperTest, UnwrapAllInvalidRecord) {
  // First record is valid, second is truncated.
  std::vector<std::uint8_t> data = {
      0x17, 0x03, 0x03, 0x00, 0x02, 0xAA, 0xBB,  // Valid 2-byte record
      0x17, 0x03, 0x03, 0x00, 0x05, 0x01           // Truncated record
  };

  auto result = TLSWrapper::unwrap_all(data);
  EXPECT_FALSE(result.has_value());
}

// Test unwrap_all with multiple valid records.
TEST(TLSWrapperTest, UnwrapAllMultipleRecords) {
  // Manually build two records.
  std::vector<std::uint8_t> data;

  // Record 1: 3 bytes payload.
  data.push_back(0x17);
  data.push_back(0x03);
  data.push_back(0x03);
  data.push_back(0x00);
  data.push_back(0x03);
  data.push_back(0x01);
  data.push_back(0x02);
  data.push_back(0x03);

  // Record 2: 2 bytes payload.
  data.push_back(0x17);
  data.push_back(0x03);
  data.push_back(0x03);
  data.push_back(0x00);
  data.push_back(0x02);
  data.push_back(0x04);
  data.push_back(0x05);

  auto result = TLSWrapper::unwrap_all(data);
  ASSERT_TRUE(result.has_value());

  std::vector<std::uint8_t> expected = {0x01, 0x02, 0x03, 0x04, 0x05};
  EXPECT_EQ(*result, expected);
}

// Test round-trip with realistic VEIL packet data.
TEST(TLSWrapperTest, RoundTripRealisticData) {
  // Simulate a VEIL packet.
  std::vector<std::uint8_t> veil_packet;
  veil_packet.push_back(0x56);  // Magic byte 'V'
  veil_packet.push_back(0x4C);  // Magic byte 'L'
  veil_packet.push_back(0x01);  // Version
  for (int i = 0; i < 100; ++i) {
    veil_packet.push_back(static_cast<std::uint8_t>(i % 256));
  }

  auto wrapped = TLSWrapper::wrap(veil_packet);

  // Verify wrapped has TLS record header.
  EXPECT_EQ(wrapped.size(), kTLSRecordHeaderSize + veil_packet.size());

  // Verify header bytes match TLS application data record.
  EXPECT_EQ(wrapped[0], 0x17);  // application_data
  EXPECT_EQ(wrapped[1], 0x03);  // TLS 1.2 major
  EXPECT_EQ(wrapped[2], 0x03);  // TLS 1.2 minor

  // Parse header.
  auto header_result = TLSWrapper::parse_header(wrapped);
  ASSERT_TRUE(header_result.has_value());
  EXPECT_EQ(header_result->first.content_type, TLSContentType::kApplicationData);
  EXPECT_EQ(header_result->first.legacy_version, 0x0303);

  // Unwrap.
  auto unwrapped = TLSWrapper::unwrap(wrapped);
  ASSERT_TRUE(unwrapped.has_value());
  EXPECT_EQ(*unwrapped, veil_packet);
}

// Test that wrapped data looks like legitimate TLS.
TEST(TLSWrapperTest, WrappedDataLooksLikeTLS) {
  std::vector<std::uint8_t> payload(200, 0xDE);
  auto wrapped = TLSWrapper::wrap(payload);

  // First byte should be 0x17 (application_data).
  EXPECT_EQ(wrapped[0], 0x17);

  // Bytes 1-2 should be 0x0303 (TLS 1.2 legacy version).
  EXPECT_EQ(wrapped[1], 0x03);
  EXPECT_EQ(wrapped[2], 0x03);

  // Bytes 3-4 should encode the payload length in big-endian.
  std::uint16_t encoded_length =
      static_cast<std::uint16_t>((wrapped[3] << 8) | wrapped[4]);
  EXPECT_EQ(encoded_length, 200);
}

// Test header overhead is exactly 5 bytes.
TEST(TLSWrapperTest, HeaderOverhead) {
  EXPECT_EQ(kTLSRecordHeaderSize, 5);

  for (std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{100},
                            std::size_t{1000}, std::size_t{16384}}) {
    std::vector<std::uint8_t> payload(size, 0x00);
    auto wrapped = TLSWrapper::wrap(payload);
    EXPECT_EQ(wrapped.size(), kTLSRecordHeaderSize + size)
        << "Failed for payload size " << size;
  }
}

// Test wrap/unwrap_all round-trip for large data.
TEST(TLSWrapperTest, RoundTripLargeData) {
  // 50000 bytes = 4 records (16384 + 16384 + 16384 + 848).
  std::vector<std::uint8_t> payload;
  payload.reserve(50000);
  for (std::size_t i = 0; i < 50000; ++i) {
    payload.push_back(static_cast<std::uint8_t>(i % 256));
  }

  auto wrapped = TLSWrapper::wrap(payload);

  // Should be 4 records.
  const std::size_t expected_records = 4;
  EXPECT_EQ(wrapped.size(), expected_records * kTLSRecordHeaderSize + payload.size());

  auto unwrapped = TLSWrapper::unwrap_all(wrapped);
  ASSERT_TRUE(unwrapped.has_value());
  EXPECT_EQ(*unwrapped, payload);
}
