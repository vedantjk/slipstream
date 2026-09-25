#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "lib/wire.h"

static_assert(wire::kSymbolLen == 12);

static_assert(
    std::is_same_v<std::underlying_type_t<wire::MsgType>, std::uint8_t>);

static_assert(sizeof(wire::Header) == 4);
static_assert(sizeof(wire::Quote) == 44);
static_assert(sizeof(wire::Trade) == 41);
static_assert(sizeof(wire::Heartbeat) == 8);
static_assert(sizeof(wire::SessionControl) == 9);
static_assert(sizeof(wire::NewOrder) == 50);

static_assert(offsetof(wire::Quote, symbol) == 0);
static_assert(offsetof(wire::Quote, ts_ns) == 12);
static_assert(offsetof(wire::Quote, bid_qty) == 20);
static_assert(offsetof(wire::Quote, bid_px) == 24);
static_assert(offsetof(wire::Quote, ask_qty) == 32);
static_assert(offsetof(wire::Quote, ask_px) == 36);
static_assert(offsetof(wire::Trade, id) == 33);
static_assert(offsetof(wire::NewOrder, ts_ns) == 21);

TEST(WireTest, UsesProtocolMessageTypeValues) {
  EXPECT_EQ(static_cast<std::uint8_t>(wire::MsgType::Quote), 1);
  EXPECT_EQ(static_cast<std::uint8_t>(wire::MsgType::Trade), 2);
  EXPECT_EQ(static_cast<std::uint8_t>(wire::MsgType::Heartbeat), 3);
  EXPECT_EQ(static_cast<std::uint8_t>(wire::MsgType::SessionControl), 4);
  EXPECT_EQ(static_cast<std::uint8_t>(wire::MsgType::NewOrder), 10);
}
