#ifndef SLIPSTREAM_MESSAGES_H
#define SLIPSTREAM_MESSAGES_H

#include <cstdint>
#include <string>
#include <variant>

enum class Aggressor : char {
  Buy = 'B',
  Sell = 'S',
  Unknown = '?',
};

enum class SessionState : std::uint8_t {
  Open = 0,
  Halt = 1,
  Close = 2,
};

enum class OrderStatus : char {
  Accepted = 'A',
  Rejected = 'R',
};

enum class Side : char {
  Buy = 'B',
  Sell = 'S',
};

struct QuoteData {
  std::uint64_t ts_ns = 0;
  std::string symbol;
  std::int64_t bid_price = 0;
  std::uint32_t bid_qty = 0;
  std::int64_t ask_price = 0;
  std::uint32_t ask_qty = 0;
};

struct TradeData {
  std::uint64_t ts_ns = 0;
  std::string symbol;
  std::int64_t trade_price = 0;
  std::uint32_t trade_qty = 0;
  Aggressor aggressor = Aggressor::Unknown;
  std::int64_t id = 0;
};

struct HeartbeatData {
  std::uint64_t ts_ns = 0;
};

struct SessionControlData {
  std::uint64_t ts_ns = 0;
  SessionState state = SessionState::Open;
};

struct NewOrderData {
  std::uint64_t client_order_id = 0;
  std::string symbol;
  OrderStatus status = OrderStatus::Rejected;
  std::uint64_t ts_ns = 0;
  std::int64_t trade_id = 0;
  Side side = Side::Buy;
  std::uint32_t qty = 0;
  std::int64_t limit_price = 0;
};

using MarketData = std::variant<QuoteData, TradeData>;
using Decoded = std::variant<QuoteData, TradeData, HeartbeatData,
                             SessionControlData, NewOrderData>;

#endif  // SLIPSTREAM_MESSAGES_H
