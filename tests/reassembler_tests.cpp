#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "lib/reassembler.h"

namespace {

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

auto makeQuoteFrame() {
  wire::Quote body{};
  std::memcpy(body.symbol, "SYNTH4", 6);
  body.ts_ns = 1'704'101'400'003'000'000ULL;
  body.bid_qty = 55;
  body.bid_px = 873'700;
  body.ask_qty = 50;
  body.ask_px = 874'900;
  return makeFrame(wire::MsgType::Quote, body);
}

auto makeHeartbeatFrame() {
  return makeFrame(wire::MsgType::Heartbeat, wire::Heartbeat{.ts_ns = 123'456});
}

auto makeNewOrderFrame() {
  wire::NewOrder body{};
  body.client_order_id = 27;
  std::memcpy(body.symbol, "SYNTH4", 6);
  body.status = 'A';
  body.ts_ns = 1'704'101'400'200'000'000ULL;
  body.trade_id = 314;
  body.side = 'B';
  body.qty = 12;
  body.limit_px = 1'234'500;
  return makeFrame(wire::MsgType::NewOrder, body);
}

TEST(ReassemblerTest, ReassemblesQuoteAtEveryPossibleSplitPoint) {
  const auto frame = makeQuoteFrame();

  for (std::size_t split = 1; split < frame.size(); ++split) {
    SCOPED_TRACE(split);
    FrameReassembler reassembler;
    std::vector<Decoded> messages;
    auto capture = [&messages](Decoded&& message) {
      messages.push_back(std::move(message));
    };

    const auto first = reassembler.push(
        std::span<const std::byte>(frame).first(split), capture);

    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, 0U);
    EXPECT_TRUE(messages.empty());

    const auto second = reassembler.push(
        std::span<const std::byte>(frame).subspan(split), capture);

    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, 1U);
    ASSERT_EQ(messages.size(), 1U);
    ASSERT_TRUE(std::holds_alternative<QuoteData>(messages.front()));
    const auto& quote = std::get<QuoteData>(messages.front());
    EXPECT_EQ(quote.symbol, "SYNTH4");
    EXPECT_EQ(quote.ts_ns, 1'704'101'400'003'000'000ULL);
    EXPECT_EQ(quote.bid_price, 873'700);
    EXPECT_EQ(quote.ask_price, 874'900);
  }
}

TEST(ReassemblerTest, ReassemblesFrameDeliveredOneByteAtATime) {
  const auto frame = makeHeartbeatFrame();
  FrameReassembler reassembler;
  std::vector<Decoded> messages;
  auto capture = [&messages](Decoded&& message) {
    messages.push_back(std::move(message));
  };

  for (std::size_t index = 0; index < frame.size(); ++index) {
    const auto result = reassembler.push(
        std::span<const std::byte>(frame).subspan(index, 1), capture);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, index + 1 == frame.size() ? 1U : 0U);
  }

  ASSERT_EQ(messages.size(), 1U);
  ASSERT_TRUE(std::holds_alternative<HeartbeatData>(messages.front()));
  EXPECT_EQ(std::get<HeartbeatData>(messages.front()).ts_ns, 123'456U);
}

TEST(ReassemblerTest, ReassemblesMaximumSizeNewOrderAtEverySplitPoint) {
  const auto frame = makeNewOrderFrame();

  for (std::size_t split = 1; split < frame.size(); ++split) {
    SCOPED_TRACE(split);
    FrameReassembler reassembler;
    std::vector<Decoded> messages;
    auto capture = [&messages](Decoded&& message) {
      messages.push_back(std::move(message));
    };

    const auto first = reassembler.push(
        std::span<const std::byte>(frame).first(split), capture);

    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, 0U);
    EXPECT_TRUE(messages.empty());

    const auto second = reassembler.push(
        std::span<const std::byte>(frame).subspan(split), capture);

    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, 1U);
    ASSERT_EQ(messages.size(), 1U);
    ASSERT_TRUE(std::holds_alternative<NewOrderData>(messages.front()));
    const auto& order = std::get<NewOrderData>(messages.front());
    EXPECT_EQ(order.client_order_id, 27U);
    EXPECT_EQ(order.symbol, "SYNTH4");
    EXPECT_EQ(order.trade_id, 314);
    EXPECT_EQ(order.side, Side::Buy);
    EXPECT_EQ(order.qty, 12U);
    EXPECT_EQ(order.limit_price, 1'234'500);
  }
}

TEST(ReassemblerTest, EmitsCoalescedFramesInWireOrder) {
  const auto quote = makeQuoteFrame();
  const auto heartbeat = makeHeartbeatFrame();
  std::array<std::byte, 60> bytes{};
  std::copy(quote.begin(), quote.end(), bytes.begin());
  std::copy(heartbeat.begin(), heartbeat.end(), bytes.begin() + quote.size());
  FrameReassembler reassembler;
  std::vector<Decoded> messages;

  const auto result = reassembler.push(bytes, [&messages](Decoded&& message) {
    messages.push_back(std::move(message));
  });

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 2U);
  ASSERT_EQ(messages.size(), 2U);
  EXPECT_TRUE(std::holds_alternative<QuoteData>(messages[0]));
  EXPECT_TRUE(std::holds_alternative<HeartbeatData>(messages[1]));
}

TEST(ReassemblerTest, RetainsPartialFrameAfterEmittingCompleteFrame) {
  const auto quote = makeQuoteFrame();
  const auto heartbeat = makeHeartbeatFrame();
  constexpr std::size_t kHeartbeatPrefixSize = 5;
  std::array<std::byte, 48 + kHeartbeatPrefixSize> first_chunk{};
  std::copy(quote.begin(), quote.end(), first_chunk.begin());
  std::copy_n(heartbeat.begin(), kHeartbeatPrefixSize,
              first_chunk.begin() + quote.size());
  FrameReassembler reassembler;
  std::vector<Decoded> messages;
  auto capture = [&messages](Decoded&& message) {
    messages.push_back(std::move(message));
  };

  const auto first = reassembler.push(first_chunk, capture);

  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(*first, 1U);
  ASSERT_EQ(messages.size(), 1U);
  EXPECT_TRUE(std::holds_alternative<QuoteData>(messages.front()));

  const auto second = reassembler.push(
      std::span<const std::byte>(heartbeat).subspan(kHeartbeatPrefixSize),
      capture);

  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(*second, 1U);
  ASSERT_EQ(messages.size(), 2U);
  EXPECT_TRUE(std::holds_alternative<HeartbeatData>(messages.back()));
}

TEST(ReassemblerTest, RemainsFailedAfterInvalidHeader) {
  const std::array<std::byte, 4> invalid_header{
      std::byte{0x08}, std::byte{0x00}, std::byte{0x03}, std::byte{0x02}};
  const auto heartbeat = makeHeartbeatFrame();
  FrameReassembler reassembler;
  std::vector<Decoded> messages;
  auto capture = [&messages](Decoded&& message) {
    messages.push_back(std::move(message));
  };

  const auto invalid = reassembler.push(invalid_header, capture);

  ASSERT_FALSE(invalid.has_value());
  EXPECT_EQ(invalid.error(), DecodeError::UnsupportedVersion);

  const auto after_error = reassembler.push(heartbeat, capture);

  ASSERT_FALSE(after_error.has_value());
  EXPECT_EQ(after_error.error(), DecodeError::UnsupportedVersion);
  EXPECT_TRUE(messages.empty());
}

TEST(ReassemblerTest, RemainsFailedAfterInvalidCompleteBody) {
  wire::Trade body{};
  body.aggressor = 'X';
  const auto invalid_frame = makeFrame(wire::MsgType::Trade, body);
  const auto quote = makeQuoteFrame();
  FrameReassembler reassembler;
  std::vector<Decoded> messages;
  auto capture = [&messages](Decoded&& message) {
    messages.push_back(std::move(message));
  };

  const auto invalid = reassembler.push(invalid_frame, capture);

  ASSERT_FALSE(invalid.has_value());
  EXPECT_EQ(invalid.error(), DecodeError::InvalidFieldValue);

  const auto after_error = reassembler.push(quote, capture);

  ASSERT_FALSE(after_error.has_value());
  EXPECT_EQ(after_error.error(), DecodeError::InvalidFieldValue);
  EXPECT_TRUE(messages.empty());
}

TEST(ReassemblerTest, EmptyChunkIsANoOp) {
  FrameReassembler reassembler;
  std::vector<Decoded> messages;

  const auto result = reassembler.push({}, [&messages](Decoded&& message) {
    messages.push_back(std::move(message));
  });

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 0U);
  EXPECT_TRUE(messages.empty());
}

}  // namespace
