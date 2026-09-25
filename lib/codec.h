#ifndef SLIPSTREAM_CODEC_H
#define SLIPSTREAM_CODEC_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "lib/parser.h"
#include "lib/wire.h"

inline std::size_t encode(const QuoteData& data, std::uint64_t base_epoch_ns,
                          std::span<std::byte> out) {
  constexpr std::size_t encoded_size =
      sizeof(wire::Header) + sizeof(wire::Quote);
  if (out.size() < encoded_size) return 0;

  const wire::Header header{.body_len = sizeof(wire::Quote),
                            .msg_type = wire::MsgType::Quote,
                            .version = 1};
  wire::Quote quote{};
  std::memcpy(quote.symbol, data.symbol.data(),
              std::min(data.symbol.size(), wire::kSymbolLen));
  quote.ts_ns = base_epoch_ns + data.timestamp;
  quote.bid_qty = data.bid_qty;
  quote.bid_px = data.bid_price;
  quote.ask_qty = data.ask_qty;
  quote.ask_px = data.ask_price;

  std::memcpy(out.data(), &header, sizeof(header));
  std::memcpy(out.data() + sizeof(header), &quote, sizeof(quote));
  return encoded_size;
}

inline std::size_t encode(const TradeData& data, std::uint64_t base_epoch_ns,
                          std::int64_t trade_id, std::span<std::byte> out) {
  constexpr std::size_t encoded_size =
      sizeof(wire::Header) + sizeof(wire::Trade);
  if (out.size() < encoded_size) return 0;

  const wire::Header header{.body_len = sizeof(wire::Trade),
                            .msg_type = wire::MsgType::Trade,
                            .version = 1};
  wire::Trade trade{};
  std::memcpy(trade.symbol, data.symbol.data(),
              std::min(data.symbol.size(), wire::kSymbolLen));
  trade.ts_ns = base_epoch_ns + data.timestamp;
  trade.qty = data.trade_qty;
  trade.px = data.trade_price;
  trade.aggressor = data.trade_side;
  trade.id = trade_id;

  std::memcpy(out.data(), &header, sizeof(header));
  std::memcpy(out.data() + sizeof(header), &trade, sizeof(trade));
  return encoded_size;
}

#endif  // SLIPSTREAM_CODEC_H
