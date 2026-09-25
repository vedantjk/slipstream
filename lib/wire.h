#ifndef SLIPSTREAM_WIRE_H
#define SLIPSTREAM_WIRE_H

#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace wire {

constexpr std::size_t kSymbolLen = 12;

static_assert(std::endian::native == std::endian::little);

enum class MsgType : std::uint8_t {
  Quote = 1,
  Trade = 2,
  Heartbeat = 3,
  SessionControl = 4,
  NewOrder = 10,
};

#pragma pack(push, 1)

struct Header {
  std::uint16_t body_len;
  MsgType msg_type;
  std::uint8_t version;
};

struct Quote {
  char symbol[kSymbolLen];
  std::uint64_t ts_ns;
  std::uint32_t bid_qty;
  std::int64_t bid_px;
  std::uint32_t ask_qty;
  std::int64_t ask_px;
};

struct Trade {
  char symbol[kSymbolLen];
  std::uint64_t ts_ns;
  std::uint32_t qty;
  std::int64_t px;
  char aggressor;
  std::int64_t id;
};

struct Heartbeat {
  std::uint64_t ts_ns;
};

struct SessionControl {
  std::uint64_t ts_ns;
  std::uint8_t state;
};

struct NewOrder {
  std::uint64_t client_order_id;
  char symbol[kSymbolLen];
  char status;
  std::uint64_t ts_ns;
  std::int64_t trade_id;
  char side;
  std::uint32_t qty;
  std::int64_t limit_px;
};

#pragma pack(pop)

static_assert(sizeof(Header) == 4);
static_assert(sizeof(Quote) == 44);
static_assert(sizeof(Trade) == 41);
static_assert(sizeof(Heartbeat) == 8);
static_assert(sizeof(SessionControl) == 9);
static_assert(sizeof(NewOrder) == 50);

static_assert(std::is_trivially_copyable_v<Header>);
static_assert(std::is_trivially_copyable_v<Quote>);
static_assert(std::is_trivially_copyable_v<Trade>);
static_assert(std::is_trivially_copyable_v<Heartbeat>);
static_assert(std::is_trivially_copyable_v<SessionControl>);
static_assert(std::is_trivially_copyable_v<NewOrder>);

}  // namespace wire

#endif  // SLIPSTREAM_WIRE_H
