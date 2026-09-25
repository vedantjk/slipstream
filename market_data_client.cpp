#include <iostream>

#include "lib/parser.h"

namespace {

// 2024-01-01T00:00:00Z. The input CSV contains UTC times of day.
constexpr std::uint64_t kBaseEpochNs = 1'704'067'200'000'000'000ULL;

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <path to data> \n";
    return 1;
  }

  const std::string filename = argv[1];
  Parser parser(filename, kBaseEpochNs);

  auto [data, skipped] = parser.parseCSV();

  if (!data) {
    std::cerr << "CSV parsing failed\n";
    return 1;
  }

  std::cout << "Invalid rows count : " << skipped << "\n";
  std::cout << "Successfully read " << data.value().size() << " records.\n";

  return 0;
}
