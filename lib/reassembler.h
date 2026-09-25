#ifndef SLIPSTREAM_REASSEMBLER_H
#define SLIPSTREAM_REASSEMBLER_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <expected>
#include <optional>
#include <span>
#include <utility>

#include "codec.h"

class FrameReassembler {
  static constexpr std::size_t kMaxBodySize = std::max(
      {sizeof(wire::Quote), sizeof(wire::Trade), sizeof(wire::Heartbeat),
       sizeof(wire::SessionControl), sizeof(wire::NewOrder)});
  static constexpr std::size_t kMaxFrameSize =
      sizeof(wire::Header) + kMaxBodySize;

  std::array<std::byte, kMaxFrameSize> frame_{};
  std::size_t buffered_ = 0;
  std::size_t frame_size_ = 0;
  std::optional<DecodeError> terminal_error_;

  void clearFrame() noexcept {
    buffered_ = 0;
    frame_size_ = 0;
  }

  void fail(DecodeError error) noexcept {
    clearFrame();
    terminal_error_ = error;
  }

 public:
  // The handler receives each message as an rvalue and must move or copy it to
  // retain it. On error, remaining input is dropped and later calls return the
  // same error.
  template <typename Handler>
  std::expected<std::size_t, DecodeError> push(std::span<const std::byte> bytes,
                                               Handler&& on_message) {
    if (terminal_error_.has_value()) {
      return std::unexpected(*terminal_error_);
    }

    std::size_t messages_emitted = 0;

    while (!bytes.empty()) {
      const std::size_t target_size =
          frame_size_ == 0 ? sizeof(wire::Header) : frame_size_;
      const std::size_t copy_size =
          std::min(target_size - buffered_, bytes.size());
      std::memcpy(frame_.data() + buffered_, bytes.data(), copy_size);
      buffered_ += copy_size;
      bytes = bytes.subspan(copy_size);

      if (buffered_ < target_size) continue;

      if (frame_size_ == 0) {
        const auto frame_size = frameSizeFromHeader(
            std::span<const std::byte>(frame_).first(sizeof(wire::Header)));
        if (!frame_size.has_value()) {
          fail(frame_size.error());
          return std::unexpected(frame_size.error());
        }
        frame_size_ = *frame_size;
        if (buffered_ < frame_size_) continue;
      }

      auto message =
          decode(std::span<const std::byte>(frame_).first(frame_size_));
      clearFrame();
      if (!message.has_value()) {
        fail(message.error());
        return std::unexpected(message.error());
      }

      on_message(std::move(*message));
      ++messages_emitted;
    }

    return messages_emitted;
  }
};

#endif  // SLIPSTREAM_REASSEMBLER_H
