#ifndef SLIPSTREAM_REPLAY_H
#define SLIPSTREAM_REPLAY_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <span>
#include <stdexcept>
#include <thread>
#include <utility>

#include "messages.h"

struct ReplayClock {
  using time_point = std::chrono::time_point<std::chrono::steady_clock>;

  [[nodiscard]] time_point now() const {
    return std::chrono::time_point_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now());
  }

  static void sleep_until(time_point deadline) {
    std::this_thread::sleep_until(deadline);
  }
};

template <typename Sink, typename Clock>
void replay(std::span<const MarketData> rows, double speed, Sink&& sink,
            Clock& clock) {
  if (!std::isfinite(speed) || speed < 0) {
    throw std::invalid_argument("replay speed must be finite and nonnegative");
  }
  const auto timestamp = [](const MarketData& row) {
    return std::visit([](const auto& value) { return value.ts_ns; }, row);
  };
  if (!std::ranges::is_sorted(rows, {}, timestamp)) {
    throw std::invalid_argument("replay timestamps must be nondecreasing");
  }
  if (rows.empty()) return;
  if (speed == 0) {
    for (const auto& row : rows) std::invoke(sink, row);
    return;
  }

  using Nanoseconds = std::chrono::nanoseconds;
  using Rep = Nanoseconds::rep;
  const auto t0 = timestamp(rows.front());
  const auto offset = [&](const MarketData& row) {
    // Subtract integers first to preserve small gaps at large epoch values.
    const long double scaled =
        static_cast<long double>(timestamp(row) - t0) / speed;
    // Use an exclusive power-of-two bound: converting max() to floating point
    // can round it up to an unrepresentable integer on some platforms.
    if (!std::isfinite(scaled) ||
        scaled >= std::ldexp(1.0L, std::numeric_limits<Rep>::digits)) {
      throw std::overflow_error("replay duration exceeds nanosecond range");
    }
    return Nanoseconds{static_cast<Rep>(scaled)};
  };
  // Sorted input and positive speed make the last offset the largest.
  const auto last_offset = offset(rows.back());
  const auto wall0 = clock.now();
  if (wall0 > ReplayClock::time_point::max() - last_offset) {
    throw std::overflow_error("replay deadline exceeds clock range");
  }
  for (const auto& row : rows) {
    const auto deadline = wall0 + offset(row);
    if (clock.now() < deadline) clock.sleep_until(deadline);
    std::invoke(sink, row);
  }
}

template <typename Sink>
void replay(std::span<const MarketData> rows, double speed, Sink&& sink) {
  ReplayClock clock;
  replay(rows, speed, std::forward<Sink>(sink), clock);
}

#endif  // SLIPSTREAM_REPLAY_H
