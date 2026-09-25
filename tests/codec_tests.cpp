#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>

#include "lib/codec.h"

namespace {

template <typename T, std::size_t N>
T readFrom(const std::array<std::byte, N>& bytes, std::size_t offset) {
  T value{};
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  return value;
}

TEST(CodecTest, EncodesQuoteHeaderAndBody) {
  constexpr std::uint64_t kBaseEpochNs = 1'700'000'000'000'000'000ULL;
  QuoteData row{.timestamp = 42,
                .symbol = "ABC",
                .bid_price = 1'234'500,
                .bid_qty = 17,
                .ask_price = 1'235'000,
                .ask_qty = 23};
  std::array<std::byte, sizeof(wire::Header) + sizeof(wire::Quote)> out{};

  const auto bytes_written = encode(row, kBaseEpochNs, out);

  EXPECT_EQ(bytes_written, 48U);

  const auto header = readFrom<wire::Header>(out, 0);
  EXPECT_EQ(header.body_len, 44);
  EXPECT_EQ(header.msg_type, wire::MsgType::Quote);
  EXPECT_EQ(header.version, 1);

  const auto quote = readFrom<wire::Quote>(out, sizeof(wire::Header));
  EXPECT_EQ(std::string_view(quote.symbol, 3), "ABC");
  EXPECT_TRUE(std::all_of(quote.symbol + 3, quote.symbol + wire::kSymbolLen,
                          [](char value) { return value == '\0'; }));
  EXPECT_EQ(quote.ts_ns, 1'700'000'000'000'000'042ULL);
  EXPECT_EQ(quote.bid_qty, 17U);
  EXPECT_EQ(quote.bid_px, 1'234'500);
  EXPECT_EQ(quote.ask_qty, 23U);
  EXPECT_EQ(quote.ask_px, 1'235'000);
}

TEST(CodecTest, ProducesSpecDefinedQuoteBytes) {
  QuoteData row{.timestamp = 0x08,
                .symbol = "AB",
                .bid_price = 0x1112131415161718,
                .bid_qty = 0x0A0B0C0D,
                .ask_price = 0x2122232425262728,
                .ask_qty = 0x1A1B1C1D};
  std::array<std::byte, 48> out{};
  const std::array expected{
      std::byte{0x2C}, std::byte{0x00}, std::byte{0x01}, std::byte{0x01},
      std::byte{0x41}, std::byte{0x42}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x08}, std::byte{0x07}, std::byte{0x06}, std::byte{0x05},
      std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01},
      std::byte{0x0D}, std::byte{0x0C}, std::byte{0x0B}, std::byte{0x0A},
      std::byte{0x18}, std::byte{0x17}, std::byte{0x16}, std::byte{0x15},
      std::byte{0x14}, std::byte{0x13}, std::byte{0x12}, std::byte{0x11},
      std::byte{0x1D}, std::byte{0x1C}, std::byte{0x1B}, std::byte{0x1A},
      std::byte{0x28}, std::byte{0x27}, std::byte{0x26}, std::byte{0x25},
      std::byte{0x24}, std::byte{0x23}, std::byte{0x22}, std::byte{0x21}};

  ASSERT_EQ(encode(row, 0x0102030405060700, out), out.size());

  EXPECT_EQ(out, expected);
}

TEST(CodecTest, DoesNotTerminateFullWidthQuoteSymbol) {
  QuoteData row{.symbol = "ABCDEFGHIJKL"};
  std::array<std::byte, sizeof(wire::Header) + sizeof(wire::Quote)> out{};

  ASSERT_EQ(encode(row, 0, out), out.size());

  const auto quote = readFrom<wire::Quote>(out, sizeof(wire::Header));
  EXPECT_EQ(std::string_view(quote.symbol, wire::kSymbolLen), "ABCDEFGHIJKL");
  EXPECT_EQ(quote.symbol[wire::kSymbolLen - 1], 'L');
}

TEST(CodecTest, TruncatesQuoteSymbolToWireWidth) {
  QuoteData row{.symbol = "ABCDEFGHIJKLM"};
  std::array<std::byte, sizeof(wire::Header) + sizeof(wire::Quote)> out{};

  ASSERT_EQ(encode(row, 0, out), out.size());

  const auto quote = readFrom<wire::Quote>(out, sizeof(wire::Header));
  EXPECT_EQ(std::string_view(quote.symbol, wire::kSymbolLen), "ABCDEFGHIJKL");
}

TEST(CodecTest, EncodesTradeHeaderBodyAndCallerSuppliedId) {
  constexpr std::uint64_t kBaseEpochNs = 1'700'000'000'000'000'000ULL;
  TradeData row{.timestamp = 99,
                .symbol = "SYNTH2",
                .trade_price = 2'485'300,
                .trade_qty = 65,
                .trade_side = 'B'};
  std::array<std::byte, sizeof(wire::Header) + sizeof(wire::Trade)> out{};

  const auto bytes_written = encode(row, kBaseEpochNs, 314, out);

  EXPECT_EQ(bytes_written, 45U);

  const auto header = readFrom<wire::Header>(out, 0);
  EXPECT_EQ(header.body_len, 41);
  EXPECT_EQ(header.msg_type, wire::MsgType::Trade);
  EXPECT_EQ(header.version, 1);

  const auto trade = readFrom<wire::Trade>(out, sizeof(wire::Header));
  EXPECT_EQ(std::string_view(trade.symbol, 6), "SYNTH2");
  EXPECT_TRUE(std::all_of(trade.symbol + 6, trade.symbol + wire::kSymbolLen,
                          [](char value) { return value == '\0'; }));
  EXPECT_EQ(trade.ts_ns, 1'700'000'000'000'000'099ULL);
  EXPECT_EQ(trade.qty, 65U);
  EXPECT_EQ(trade.px, 2'485'300);
  EXPECT_EQ(trade.aggressor, 'B');
  EXPECT_EQ(trade.id, 314);
}

TEST(CodecTest, ProducesSpecDefinedTradeBytes) {
  TradeData row{.timestamp = 0x08,
                .symbol = "T",
                .trade_price = 0x1112131415161718,
                .trade_qty = 0x0A0B0C0D,
                .trade_side = 'S'};
  std::array<std::byte, 45> out{};
  const std::array<std::byte, 45> expected{
      std::byte{0x29}, std::byte{0x00}, std::byte{0x02}, std::byte{0x01},
      std::byte{0x54}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
      std::byte{0x08}, std::byte{0x07}, std::byte{0x06}, std::byte{0x05},
      std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01},
      std::byte{0x0D}, std::byte{0x0C}, std::byte{0x0B}, std::byte{0x0A},
      std::byte{0x18}, std::byte{0x17}, std::byte{0x16}, std::byte{0x15},
      std::byte{0x14}, std::byte{0x13}, std::byte{0x12}, std::byte{0x11},
      std::byte{0x53}, std::byte{0x28}, std::byte{0x27}, std::byte{0x26},
      std::byte{0x25}, std::byte{0x24}, std::byte{0x23}, std::byte{0x22},
      std::byte{0x21}};

  ASSERT_EQ(encode(row, 0x0102030405060700, 0x2122232425262728, out),
            out.size());

  EXPECT_EQ(out, expected);
}

TEST(CodecTest, ReturnsZeroForShortOutputBuffers) {
  QuoteData quote{};
  TradeData trade{};
  std::array<std::byte, sizeof(wire::Header) + sizeof(wire::Quote) - 1>
      quote_out{};
  std::array<std::byte, sizeof(wire::Header) + sizeof(wire::Trade) - 1>
      trade_out{};

  EXPECT_EQ(encode(quote, 0, quote_out), 0U);
  EXPECT_EQ(encode(trade, 0, 1, trade_out), 0U);
}

}  // namespace
