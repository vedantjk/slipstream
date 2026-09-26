# Slipstream

A C++23 single-instrument execution server. Two clients replay a historical CSV
over TCP: one sends quotes, the other sends the firm's own trades. The server
keeps a top-of-book and a rolling VWAP (volume-weighted average price), judges
each trade against a price band and a participation cap, replies accepted or
rejected, and prints an execution report with tick-to-order latency percentiles.

**Current status:** the data layer is built and tested: CSV parser, wire structs,
codec, frame reassembler and replay scheduler. Still to come: the replay clients,
socket transport, book and VWAP, decision logic, and latency measurements.

## Build and run

GCC 13 or newer on Linux, CMake 3.20+, network access on first configure for
GoogleTest.

```sh
cmake -S . -B build -DCMAKE_CXX_COMPILER=g++
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/market_data_client data/QandT.csv   # parses and counts; no replay yet
```

## Design notes

- **Wire structs** (`lib/wire.h`) are packed and little-endian, with a
  `static_assert` on every size and on the host byte order. Unpacked, a Quote is
  56 bytes against the protocol's 44.
- **Codec** (`lib/codec.h`) validates version, type, body length, frame length and
  enum fields before returning a message; errors come back as `std::expected`.
  All buffer access is `memcpy`, since several fields are unaligned.
- **Reassembler** (`lib/reassembler.h`) rebuilds frames from arbitrary chunks. The
  header is validated before the body is awaited, so a corrupt length cannot stall
  the stream; storage is a fixed 54 bytes. The first decode error poisons the
  stream, because framing cannot be recovered once lost.
- **Replay** (`lib/replay.h`) anchors every deadline to the first row,
  `start + (ts - ts0) / speed`, so sleep overshoot does not accumulate. A full
  10,000-row replay at 600x and 3600x finishes within 70 microseconds of schedule.

70 tests cover CSV validation, spec-derived wire bytes, decoder rejections, frames
split at every byte offset, the poisoned-stream rule, and replay timing.

## Assumptions

- Trade body is 41 bytes and NewOrder is 50, from summing the field tables.
- CSV: LF endings, ten unquoted fields, aggressor `B`/`S` in column 10. Times are
  `HH:MM:SS.mmm` UTC on 2024-01-01, no midnight crossing.
- Prices parse as `double`, scale by 10,000 and round; exact for all 16,905 prices
  in the supplied file.
- Trade IDs are zero for now; row-number assignment is planned.

## Known issues and open questions

- Clang 18 on Ubuntu 24.04 cannot build this: libstdc++ 13 hides `std::expected`
  from Clang. Use GCC.
- No end-of-stream call on the reassembler, so a truncated final frame is silent.
- The encoder truncates symbols over 12 characters; the parser rejects them first.
- CRLF, surrounding whitespace and numeric nanosecond timestamps are unsupported.
- For the brief's authors: validator or live execution algorithm; the wire-size
  discrepancy; cumulative or windowed participation cap; silent or always-reply
  during warm-up.
