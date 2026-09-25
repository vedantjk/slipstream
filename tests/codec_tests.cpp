#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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

template <typename Body>
std::array<std::byte, sizeof(wire::Header) + sizeof(Body)> makeFrame(
    wire::MsgType type, const Body& body) {
  const wire::Header header{
      .body_len = sizeof(Body), .msg_type = type, .version = 1};
  std::array<std::byte, sizeof(wire::Header) + sizeof(Body)> frame{};
  std::memcpy(frame.data(), &header, sizeof(header));
  std::memcpy(frame.data() + sizeof(header), &body, sizeof(body));
  return frame;
}

TEST(CodecTest, EncodesQuoteHeaderAndBody) {
  QuoteData row{.ts_ns = 1'700'000'000'000'000'042ULL,
                .symbol = "ABC",
                .bid_price = 1'234'500,
                .bid_qty = 17,
                .ask_price = 1'235'000,
                .ask_qty = 23};
  std::array<std::byte, sizeof(wire::Header) + sizeof(wire::Quote)> out{};

  const auto bytes_written = encode(row, out);

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
  QuoteData row{.ts_ns = 0x0102030405060708,
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

  ASSERT_EQ(encode(row, out), out.size());

  EXPECT_EQ(out, expected);
}

TEST(CodecTest, DoesNotTerminateFullWidthQuoteSymbol) {
  QuoteData row{.symbol = "ABCDEFGHIJKL"};
  std::array<std::byte, sizeof(wire::Header) + sizeof(wire::Quote)> out{};

  ASSERT_EQ(encode(row, out), out.size());

  const auto quote = readFrom<wire::Quote>(out, sizeof(wire::Header));
  EXPECT_EQ(std::string_view(quote.symbol, wire::kSymbolLen), "ABCDEFGHIJKL");
  EXPECT_EQ(quote.symbol[wire::kSymbolLen - 1], 'L');
}

TEST(CodecTest, TruncatesQuoteSymbolToWireWidth) {
  QuoteData row{.symbol = "ABCDEFGHIJKLM"};
  std::array<std::byte, sizeof(wire::Header) + sizeof(wire::Quote)> out{};

  ASSERT_EQ(encode(row, out), out.size());

  const auto quote = readFrom<wire::Quote>(out, sizeof(wire::Header));
  EXPECT_EQ(std::string_view(quote.symbol, wire::kSymbolLen), "ABCDEFGHIJKL");
}

TEST(CodecTest, EncodesTradeHeaderBodyAndCallerSuppliedId) {
  TradeData row{.ts_ns = 1'700'000'000'000'000'099ULL,
                .symbol = "SYNTH2",
                .trade_price = 2'485'300,
                .trade_qty = 65,
                .aggressor = Aggressor::Buy,
                .id = 314};
  std::array<std::byte, sizeof(wire::Header) + sizeof(wire::Trade)> out{};

  const auto bytes_written = encode(row, out);

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
  TradeData row{.ts_ns = 0x0102030405060708,
                .symbol = "T",
                .trade_price = 0x1112131415161718,
                .trade_qty = 0x0A0B0C0D,
                .aggressor = Aggressor::Sell,
                .id = 0x2122232425262728};
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

  ASSERT_EQ(encode(row, out), out.size());

  EXPECT_EQ(out, expected);
}

TEST(CodecTest, ReturnsZeroForShortOutputBuffers) {
  QuoteData quote{};
  TradeData trade{};
  std::array<std::byte, sizeof(wire::Header) + sizeof(wire::Quote) - 1>
      quote_out{};
  std::array<std::byte, sizeof(wire::Header) + sizeof(wire::Trade) - 1>
      trade_out{};

  EXPECT_EQ(encode(quote, quote_out), 0U);
  EXPECT_EQ(encode(trade, trade_out), 0U);
}

TEST(CodecTest, ReportsBodyLengthForKnownMessageTypes) {
  EXPECT_EQ(bodyLengthFor(1), sizeof(wire::Quote));
  EXPECT_EQ(bodyLengthFor(2), sizeof(wire::Trade));
  EXPECT_EQ(bodyLengthFor(3), sizeof(wire::Heartbeat));
  EXPECT_EQ(bodyLengthFor(4), sizeof(wire::SessionControl));
  EXPECT_EQ(bodyLengthFor(10), sizeof(wire::NewOrder));
  EXPECT_FALSE(bodyLengthFor(11).has_value());
}

TEST(CodecTest, ReportsFrameSizeFromValidHeader) {
  const auto frame =
      makeFrame(wire::MsgType::Heartbeat, wire::Heartbeat{.ts_ns = 123'456});

  const auto frame_size = frameSizeFromHeader(
      std::span<const std::byte>(frame).first(sizeof(wire::Header)));

  ASSERT_TRUE(frame_size.has_value());
  EXPECT_EQ(*frame_size, 12U);
}

TEST(CodecTest, DecodesQuote) {
  wire::Quote body{};
  std::memcpy(body.symbol, "ABCDEFGHIJKL", wire::kSymbolLen);
  body.ts_ns = 1'704'101'400'003'000'000ULL;
  body.bid_qty = 55;
  body.bid_px = 873'700;
  body.ask_qty = 50;
  body.ask_px = 874'900;
  const auto frame = makeFrame(wire::MsgType::Quote, body);

  const auto decoded = decode(frame);

  ASSERT_TRUE(decoded.has_value());
  ASSERT_TRUE(std::holds_alternative<QuoteData>(*decoded));
  const auto& quote = std::get<QuoteData>(*decoded);
  EXPECT_EQ(quote.ts_ns, 1'704'101'400'003'000'000ULL);
  EXPECT_EQ(quote.symbol, "ABCDEFGHIJKL");
  EXPECT_EQ(quote.bid_price, 873'700);
  EXPECT_EQ(quote.bid_qty, 55U);
  EXPECT_EQ(quote.ask_price, 874'900);
  EXPECT_EQ(quote.ask_qty, 50U);
}

TEST(CodecTest, DecodesTradeAndPreservesId) {
  wire::Trade body{};
  std::memcpy(body.symbol, "SYNTH2", 6);
  body.ts_ns = 1'704'101'400'190'000'000ULL;
  body.qty = 65;
  body.px = 2'485'300;
  body.aggressor = '?';
  body.id = 314;
  const auto frame = makeFrame(wire::MsgType::Trade, body);

  const auto decoded = decode(frame);

  ASSERT_TRUE(decoded.has_value());
  ASSERT_TRUE(std::holds_alternative<TradeData>(*decoded));
  const auto& trade = std::get<TradeData>(*decoded);
  EXPECT_EQ(trade.ts_ns, 1'704'101'400'190'000'000ULL);
  EXPECT_EQ(trade.symbol, "SYNTH2");
  EXPECT_EQ(trade.trade_price, 2'485'300);
  EXPECT_EQ(trade.trade_qty, 65U);
  EXPECT_EQ(trade.aggressor, Aggressor::Unknown);
  EXPECT_EQ(trade.id, 314);
}

TEST(CodecTest, DecodesHeartbeat) {
  const auto frame =
      makeFrame(wire::MsgType::Heartbeat, wire::Heartbeat{.ts_ns = 123'456});

  const auto decoded = decode(frame);

  ASSERT_TRUE(decoded.has_value());
  ASSERT_TRUE(std::holds_alternative<HeartbeatData>(*decoded));
  EXPECT_EQ(std::get<HeartbeatData>(*decoded).ts_ns, 123'456U);
}

TEST(CodecTest, DecodesSessionControl) {
  const auto frame =
      makeFrame(wire::MsgType::SessionControl,
                wire::SessionControl{.ts_ns = 123'456, .state = 1});

  const auto decoded = decode(frame);

  ASSERT_TRUE(decoded.has_value());
  ASSERT_TRUE(std::holds_alternative<SessionControlData>(*decoded));
  const auto& control = std::get<SessionControlData>(*decoded);
  EXPECT_EQ(control.ts_ns, 123'456U);
  EXPECT_EQ(control.state, SessionState::Halt);
}

TEST(CodecTest, DecodesNewOrder) {
  wire::NewOrder body{};
  body.client_order_id = 27;
  std::memcpy(body.symbol, "ABCDEFGHIJKL", wire::kSymbolLen);
  body.status = 'A';
  body.ts_ns = 1'704'101'400'200'000'000ULL;
  body.trade_id = 314;
  body.side = 'S';
  body.qty = 12;
  body.limit_px = 1'234'500;
  const auto frame = makeFrame(wire::MsgType::NewOrder, body);

  const auto decoded = decode(frame);

  ASSERT_TRUE(decoded.has_value());
  ASSERT_TRUE(std::holds_alternative<NewOrderData>(*decoded));
  const auto& order = std::get<NewOrderData>(*decoded);
  EXPECT_EQ(order.client_order_id, 27U);
  EXPECT_EQ(order.symbol, "ABCDEFGHIJKL");
  EXPECT_EQ(order.status, OrderStatus::Accepted);
  EXPECT_EQ(order.ts_ns, 1'704'101'400'200'000'000ULL);
  EXPECT_EQ(order.trade_id, 314);
  EXPECT_EQ(order.side, Side::Sell);
  EXPECT_EQ(order.qty, 12U);
  EXPECT_EQ(order.limit_price, 1'234'500);
}

TEST(CodecTest, RejectsFrameShorterThanHeader) {
  const std::array<std::byte, 3> frame{};

  const auto decoded = decode(frame);

  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error(), DecodeError::FrameTooShort);
}

TEST(CodecTest, RejectsUnsupportedVersion) {
  const std::array<std::byte, 4> frame{std::byte{0x08}, std::byte{0x00},
                                       std::byte{0x03}, std::byte{0x02}};

  const auto decoded = decode(frame);

  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error(), DecodeError::UnsupportedVersion);
}

TEST(CodecTest, RejectsUnknownMessageTypeByte) {
  const std::array<std::byte, 4> frame{std::byte{0x00}, std::byte{0x00},
                                       std::byte{0xFF}, std::byte{0x01}};

  const auto decoded = decode(frame);

  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error(), DecodeError::UnknownMessageType);
}

TEST(CodecTest, RejectsWrongBodyLengthForEveryKnownType) {
  constexpr std::array<std::uint8_t, 5> kKnownTypes{1, 2, 3, 4, 10};

  for (const std::uint8_t type : kKnownTypes) {
    const std::array<std::byte, 4> frame{std::byte{0x00}, std::byte{0x00},
                                         std::byte{type}, std::byte{0x01}};

    const auto decoded = decode(frame);

    ASSERT_FALSE(decoded.has_value()) << "type=" << static_cast<int>(type);
    EXPECT_EQ(decoded.error(), DecodeError::BodyLengthMismatch)
        << "type=" << static_cast<int>(type);
  }
}

TEST(CodecTest, RejectsTruncatedOrTrailingFrameBytes) {
  const auto valid =
      makeFrame(wire::MsgType::Heartbeat, wire::Heartbeat{.ts_ns = 123'456});
  std::array<std::byte, sizeof(valid) + 1> trailing{};
  std::copy(valid.begin(), valid.end(), trailing.begin());

  const auto truncated =
      decode(std::span<const std::byte>(valid).first(valid.size() - 1));
  const auto oversized = decode(trailing);

  ASSERT_FALSE(truncated.has_value());
  EXPECT_EQ(truncated.error(), DecodeError::FrameLengthMismatch);
  ASSERT_FALSE(oversized.has_value());
  EXPECT_EQ(oversized.error(), DecodeError::FrameLengthMismatch);
}

TEST(CodecTest, RejectsInvalidTradeAggressor) {
  wire::Trade body{};
  body.aggressor = 'X';
  const auto frame = makeFrame(wire::MsgType::Trade, body);

  const auto decoded = decode(frame);

  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error(), DecodeError::InvalidFieldValue);
}

TEST(CodecTest, RejectsInvalidSessionState) {
  const auto frame =
      makeFrame(wire::MsgType::SessionControl,
                wire::SessionControl{.ts_ns = 123'456, .state = 3});

  const auto decoded = decode(frame);

  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error(), DecodeError::InvalidFieldValue);
}

TEST(CodecTest, RejectsInvalidOrderStatus) {
  wire::NewOrder body{};
  body.status = 'X';
  body.side = 'B';
  const auto frame = makeFrame(wire::MsgType::NewOrder, body);

  const auto decoded = decode(frame);

  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error(), DecodeError::InvalidFieldValue);
}

TEST(CodecTest, RejectsInvalidOrderSide) {
  wire::NewOrder body{};
  body.status = 'A';
  body.side = '?';
  const auto frame = makeFrame(wire::MsgType::NewOrder, body);

  const auto decoded = decode(frame);

  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error(), DecodeError::InvalidFieldValue);
}

}  // namespace
