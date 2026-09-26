#include "codec.h"

#include <algorithm>
#include <cstring>
#include <utility>

std::size_t encode(const QuoteData& data, std::span<std::byte> out) {
  constexpr std::size_t encoded_size =
      sizeof(wire::Header) + sizeof(wire::Quote);
  if (out.size() < encoded_size) return 0;

  const wire::Header header{.body_len = sizeof(wire::Quote),
                            .msg_type = wire::MsgType::Quote,
                            .version = 1};
  wire::Quote quote{};
  std::memcpy(quote.symbol, data.symbol.data(),
              std::min(data.symbol.size(), wire::kSymbolLen));
  quote.ts_ns = data.ts_ns;
  quote.bid_qty = data.bid_qty;
  quote.bid_px = data.bid_price;
  quote.ask_qty = data.ask_qty;
  quote.ask_px = data.ask_price;

  std::memcpy(out.data(), &header, sizeof(header));
  std::memcpy(out.data() + sizeof(header), &quote, sizeof(quote));
  return encoded_size;
}

std::size_t encode(const TradeData& data, std::span<std::byte> out) {
  constexpr std::size_t encoded_size =
      sizeof(wire::Header) + sizeof(wire::Trade);
  if (out.size() < encoded_size) return 0;

  const wire::Header header{.body_len = sizeof(wire::Trade),
                            .msg_type = wire::MsgType::Trade,
                            .version = 1};
  wire::Trade trade{};
  std::memcpy(trade.symbol, data.symbol.data(),
              std::min(data.symbol.size(), wire::kSymbolLen));
  trade.ts_ns = data.ts_ns;
  trade.qty = data.trade_qty;
  trade.px = data.trade_price;
  trade.aggressor = static_cast<char>(data.aggressor);
  trade.id = data.id;

  std::memcpy(out.data(), &header, sizeof(header));
  std::memcpy(out.data() + sizeof(header), &trade, sizeof(trade));
  return encoded_size;
}

std::expected<std::size_t, DecodeError> frameSizeFromHeader(
    std::span<const std::byte> bytes) {
  if (bytes.size() < sizeof(wire::Header)) {
    return std::unexpected(DecodeError::FrameTooShort);
  }

  wire::Header header{};
  std::memcpy(&header, bytes.data(), sizeof(header));

  if (header.version != 1) {
    return std::unexpected(DecodeError::UnsupportedVersion);
  }

  const std::uint8_t raw_type = std::to_integer<std::uint8_t>(bytes[2]);
  const auto expected_body_len = bodyLengthFor(raw_type);
  if (!expected_body_len.has_value()) {
    return std::unexpected(DecodeError::UnknownMessageType);
  }

  if (header.body_len != *expected_body_len) {
    return std::unexpected(DecodeError::BodyLengthMismatch);
  }

  return sizeof(wire::Header) + *expected_body_len;
}

namespace codec_detail {

template <typename Body>
Body readBody(std::span<const std::byte> frame) {
  Body body{};
  std::memcpy(&body, frame.data() + sizeof(wire::Header), sizeof(body));
  return body;
}

std::string readSymbol(const char (&symbol)[wire::kSymbolLen]) {
  const auto end = std::find(symbol, symbol + wire::kSymbolLen, '\0');
  return {symbol, end};
}

}  // namespace codec_detail

std::expected<Decoded, DecodeError> decode(std::span<const std::byte> frame) {
  const auto expected_frame_size = frameSizeFromHeader(frame);
  if (!expected_frame_size.has_value()) {
    return std::unexpected(expected_frame_size.error());
  }

  if (frame.size() != *expected_frame_size) {
    return std::unexpected(DecodeError::FrameLengthMismatch);
  }

  const std::uint8_t raw_type = std::to_integer<std::uint8_t>(frame[2]);
  switch (raw_type) {
    case 1: {
      const auto body = codec_detail::readBody<wire::Quote>(frame);
      QuoteData quote{.ts_ns = body.ts_ns,
                      .symbol = codec_detail::readSymbol(body.symbol),
                      .bid_price = body.bid_px,
                      .bid_qty = body.bid_qty,
                      .ask_price = body.ask_px,
                      .ask_qty = body.ask_qty};
      return Decoded{std::move(quote)};
    }
    case 2: {
      const auto body = codec_detail::readBody<wire::Trade>(frame);
      Aggressor aggressor;
      switch (body.aggressor) {
        case 'B':
          aggressor = Aggressor::Buy;
          break;
        case 'S':
          aggressor = Aggressor::Sell;
          break;
        case '?':
          aggressor = Aggressor::Unknown;
          break;
        default:
          return std::unexpected(DecodeError::InvalidFieldValue);
      }

      TradeData trade{.ts_ns = body.ts_ns,
                      .symbol = codec_detail::readSymbol(body.symbol),
                      .trade_price = body.px,
                      .trade_qty = body.qty,
                      .aggressor = aggressor,
                      .id = body.id};
      return Decoded{std::move(trade)};
    }
    case 3: {
      const auto body = codec_detail::readBody<wire::Heartbeat>(frame);
      return Decoded{HeartbeatData{.ts_ns = body.ts_ns}};
    }
    case 4: {
      const auto body = codec_detail::readBody<wire::SessionControl>(frame);
      if (body.state > 2) {
        return std::unexpected(DecodeError::InvalidFieldValue);
      }
      return Decoded{SessionControlData{
          .ts_ns = body.ts_ns, .state = static_cast<SessionState>(body.state)}};
    }
    case 10: {
      const auto body = codec_detail::readBody<wire::NewOrder>(frame);
      OrderStatus status;
      if (body.status == 'A') {
        status = OrderStatus::Accepted;
      } else if (body.status == 'R') {
        status = OrderStatus::Rejected;
      } else {
        return std::unexpected(DecodeError::InvalidFieldValue);
      }

      Side side;
      if (body.side == 'B') {
        side = Side::Buy;
      } else if (body.side == 'S') {
        side = Side::Sell;
      } else {
        return std::unexpected(DecodeError::InvalidFieldValue);
      }

      NewOrderData order{.client_order_id = body.client_order_id,
                         .symbol = codec_detail::readSymbol(body.symbol),
                         .status = status,
                         .ts_ns = body.ts_ns,
                         .trade_id = body.trade_id,
                         .side = side,
                         .qty = body.qty,
                         .limit_price = body.limit_px};
      return Decoded{std::move(order)};
    }
    default:
      return std::unexpected(DecodeError::UnknownMessageType);
  }
}
