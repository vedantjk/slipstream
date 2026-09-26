#include <gtest/gtest.h>

#include <limits>
#include <vector>

#include "lib/replay.h"

using namespace std::chrono_literals;

namespace {
struct FakeClock {
  ReplayClock::time_point current{5s};
  std::chrono::nanoseconds jitter{0};
  std::vector<ReplayClock::time_point> sleeps;

  auto now() const { return current; }
  void sleep_until(ReplayClock::time_point deadline) {
    sleeps.push_back(deadline);
    current = std::max(current, deadline) + jitter;
  }
};

std::vector<MarketData> rowsAt(std::initializer_list<std::uint64_t> times) {
  std::vector<MarketData> rows;
  for (auto ts : times) {
    QuoteData row;
    row.ts_ns = ts;
    rows.emplace_back(row);
  }
  return rows;
}
}  // namespace

TEST(Replay, AnchorsTenThousandDeadlinesDespiteJitterAndSinkWork) {
  std::vector<MarketData> rows;
  constexpr std::uint64_t epoch = 1'704'067'200'000'000'000ULL;
  for (std::uint64_t i = 0; i < 10'000; ++i) {
    TradeData row;
    row.ts_ns = epoch + i * 60'000'000;
    rows.emplace_back(row);
  }
  FakeClock clock;
  clock.jitter = 20us;
  const auto anchor = clock.now();
  std::size_t delivered = 0;
  replay(
      rows, 60,
      [&](const MarketData& row) {
        EXPECT_EQ(&row, &rows[delivered++]);
        clock.current += 10us;
      },
      clock);
  EXPECT_EQ(delivered, rows.size());
  ASSERT_EQ(clock.sleeps.size(), rows.size() - 1);
  for (std::size_t i = 0; i < clock.sleeps.size(); ++i)
    EXPECT_EQ(clock.sleeps[i], anchor + (i + 1) * 1ms);
}

TEST(Replay, PreservesEqualTimestampsAndSkipsOverdueWaits) {
  auto rows = rowsAt({100, 100, 110, 200});
  FakeClock clock;
  const auto anchor = clock.now();
  std::vector<ReplayClock::time_point> arrivals;
  replay(
      rows, 1,
      [&](const MarketData&) {
        arrivals.push_back(clock.now());
        clock.current += 20ns;
      },
      clock);
  EXPECT_EQ(arrivals,
            (std::vector<ReplayClock::time_point>{
                anchor, anchor + 20ns, anchor + 40ns, anchor + 100ns}));
  EXPECT_EQ(clock.sleeps, (std::vector{anchor + 100ns}));
}

TEST(Replay, ScalesFractionalSpeedAndTruncatesFractionalNanoseconds) {
  for (const auto speed : {0.5, 2.0}) {
    auto rows = rowsAt({100, 103});
    FakeClock clock;
    const auto anchor = clock.now();
    replay(rows, speed, [](const MarketData&) {}, clock);
    ASSERT_EQ(clock.sleeps.size(), 1);
    EXPECT_EQ(clock.sleeps[0], anchor + (speed == 0.5 ? 6ns : 1ns));
  }
}

TEST(Replay, HandlesEmptySingleAndMaxRateInputs) {
  for (auto rows : {rowsAt({}), rowsAt({10}), rowsAt({10, 20, 30})}) {
    FakeClock clock;
    std::size_t count = 0;
    replay(
        rows, 0,
        [&](const MarketData& row) { EXPECT_EQ(&row, &rows[count++]); }, clock);
    EXPECT_EQ(count, rows.size());
    EXPECT_TRUE(clock.sleeps.empty());
  }
  FakeClock clock;
  replay(rowsAt({}), 1, [](const MarketData&) { FAIL(); }, clock);
  int count = 0;
  replay(rowsAt({10}), 1, [&](const MarketData&) { ++count; }, clock);
  EXPECT_EQ(count, 1);
  EXPECT_TRUE(clock.sleeps.empty());
}

TEST(Replay, RejectsInvalidInputsBeforeCallingSink) {
  FakeClock clock;
  auto sink = [](const MarketData&) { ADD_FAILURE(); };
  for (double speed : {-1.0, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
    EXPECT_THROW(replay(rowsAt({10}), speed, sink, clock),
                 std::invalid_argument);
  }
  for (double speed : {0.0, 1.0}) {
    EXPECT_THROW(replay(rowsAt({20, 10}), speed, sink, clock),
                 std::invalid_argument);
  }
  EXPECT_TRUE(clock.sleeps.empty());
}

TEST(Replay, RejectsUnrepresentableDurationAndDeadlineBeforeDelivery) {
  FakeClock clock;
  auto sink = [](const MarketData&) { ADD_FAILURE(); };
  EXPECT_THROW(replay(rowsAt({0, UINT64_MAX}), 1, sink, clock),
               std::overflow_error);
  EXPECT_THROW(replay(rowsAt({0, 1}), std::numeric_limits<double>::denorm_min(),
                      sink, clock),
               std::overflow_error);
  clock.current = ReplayClock::time_point::max() - 5ns;
  EXPECT_THROW(replay(rowsAt({0, 10}), 1, sink, clock), std::overflow_error);
  // A deadline exactly at the maximum is still valid.
  int count = 0;
  replay(rowsAt({0, 5}), 1, [&](const MarketData&) { ++count; }, clock);
  EXPECT_EQ(count, 2);
}

TEST(Replay, PropagatesSinkExceptionsWithoutSendingMoreRows) {
  FakeClock clock;
  int count = 0;
  EXPECT_THROW(replay(
                   rowsAt({0, 10}), 1,
                   [&](const MarketData&) {
                     ++count;
                     throw std::runtime_error("sink failed");
                   },
                   clock),
               std::runtime_error);
  EXPECT_EQ(count, 1);
  EXPECT_TRUE(clock.sleeps.empty());
}

TEST(Replay, RealClockDoesNotDeliverLastRowEarly) {
  auto rows = rowsAt({0, 200'000'000});
  const auto start = ReplayClock{}.now();
  std::vector<ReplayClock::time_point> arrivals;
  replay(rows, 1,
         [&](const MarketData&) { arrivals.push_back(ReplayClock{}.now()); });
  ASSERT_EQ(arrivals.size(), 2);
  // Compare counts to avoid instantiating chrono formatting in GTest's
  // failure printer (which some clangd/libstdc++ combinations reject).
  const auto elapsed = arrivals.back() - start;
  EXPECT_GE(elapsed.count(), std::chrono::nanoseconds{200ms}.count());
  // No tight upper bound: OS scheduling latency is not a correctness guarantee.
}
