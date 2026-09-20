#include <iostream>

#include "lib/parser.h"
int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <path to data> \n";
    return 1;
  }

  const std::string filename = argv[1];
  Parser parser(filename);

  auto [data, skipped] = parser.parseCSV();

  if (!data) {
    std::cerr << "CSV parsing failed\n";
    return 1;
  }

  std::cout << "Invalid rows count : " << skipped << "\n";
  std::cout << "Successfully read " << data.value().size() << " records.\n";

  return 0;
}