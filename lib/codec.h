#ifndef SLIPSTREAM_CODEC_H
#define SLIPSTREAM_CODEC_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

#include "messages.h"
#include "wire.h"

// Returns bytes written, or zero when the output buffer is too small.
std::size_t encode(const QuoteData& data, std::span<std::byte> out);
std::size_t encode(const TradeData& data, std::span<std::byte> out);

enum class DecodeError {
  FrameTooShort,
  UnsupportedVersion,
  UnknownMessageType,
  BodyLengthMismatch,
  FrameLengthMismatch,
  InvalidFieldValue,
};

inline constexpr std::optional<std::uint16_t> bodyLengthFor(
    std::uint8_t raw_type) {
  switch (raw_type) {
    case 1:
      return sizeof(wire::Quote);
    case 2:
      return sizeof(wire::Trade);
    case 3:
      return sizeof(wire::Heartbeat);
    case 4:
      return sizeof(wire::SessionControl);
    case 10:
      return sizeof(wire::NewOrder);
    default:
      return std::nullopt;
  }
}

std::expected<std::size_t, DecodeError> frameSizeFromHeader(
    std::span<const std::byte> bytes);
std::expected<Decoded, DecodeError> decode(std::span<const std::byte> frame);

#endif  // SLIPSTREAM_CODEC_H
