#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include <array>
#include <string_view>
#include <iostream>

#ifndef SLIPSTREAM_PARSER_H
#define SLIPSTREAM_PARSER_H

struct QuoteData
{
    std::string timestamp;
    std::string symbol;
    int64_t bid_price = 0;
    uint32_t bid_qty = 0;
    int64_t ask_price = 0;
    uint32_t ask_qty = 0;
};

struct TradeData
{
    std::string timestamp;
    std::string symbol;
    int64_t trade_price = 0;
    uint32_t trade_qty = 0;
    char trade_side = ' '; // 'S' or 'B'
};

using MarketData = std::variant<QuoteData, TradeData>;

struct ParserResult
{
  std::optional<std::vector<MarketData>> data;
  std::size_t invalid_parse_count = 0;
};

class Parser
{
    std::string filename_;
    using Cells = std::array<std::string_view, 10>;


    static std::optional<Cells> splitRow(std::string_view line)
    {
        Cells cells;
        std::size_t begin = 0;
        for (std::size_t i = 0; i< cells.size() - 1; i++)
        {
            const std::size_t comma = line.find(',', begin);

            if (comma == std::string_view::npos)
            {
                return std::nullopt;
            }

            cells[i] = line.substr(begin, comma - begin);
            begin = comma + 1;
        }

        if (line.find(',', begin) != std::string_view::npos) return std::nullopt;
        cells.back() = line.substr(begin);
        return cells;
    }

    static bool parsePrice(const std::string& input, std::int64_t& output) {
        try {
            std::size_t pos = 0;
            double price = std::stod(input, &pos);

            if (pos != input.size() || !std::isfinite(price)) return false;

            double scaled = price * 10000;

            constexpr double minValue =
              static_cast<double>(std::numeric_limits<std::int64_t>::min());

            constexpr double maxExclusive = -minValue;

            if (scaled < minValue || scaled >= maxExclusive) {
                return false;
            }

            output = static_cast<int64_t>(std::llround(scaled));
            return true;
        }
        catch (const std::invalid_argument&) {
            return false;
        }
        catch (const std::out_of_range&) {
            return false;
        }
    }

    static bool parseQty(const std::string& input, uint32_t& output) {
        try {
            if (input.empty() || input.front() == '-') return false;

            std::size_t pos = 0;
            unsigned long long value = std::stoull(input, &pos);

            if (pos!=input.size()) return false;

            if (value > std::numeric_limits<uint32_t>::max()) return false;

            output = static_cast<uint32_t>(value);
            return true;
        }
        catch (const std::invalid_argument&) {
            return false;
        }
        catch (const std::out_of_range&) {
            return false;
        }
    }

public:

    Parser(std::string filename) : filename_(std::move(filename)){}

    static std::optional<MarketData> parse(std::string_view line) {
        auto result = splitRow(line);

        if (!result.has_value()) return std::nullopt;

        const Cells& cells = *result;

        if (cells[0].empty() || cells[2].empty()) return std::nullopt;
        std::string_view timestamp = cells[0];
        std::string_view symbol = cells[2];

        if (symbol.size() > 12) return std::nullopt;

        if (cells[1] == "Q")
        {
            QuoteData quote_data;
            quote_data.timestamp = timestamp;
            quote_data.symbol = symbol;
            if (cells[3].empty() || cells[4].empty() || cells[5].empty() || cells[6].empty()) return std::nullopt;
            if (!cells[7].empty() || !cells[8].empty() || !cells[9].empty()) return std::nullopt;

            if (!parsePrice(std::string(cells[3]), quote_data.bid_price) ||
                !parseQty(std::string(cells[4]), quote_data.bid_qty) ||
                !parsePrice(std::string(cells[5]), quote_data.ask_price) ||
                !parseQty(std::string(cells[6]), quote_data.ask_qty)) {
                return std::nullopt;
                }
            return quote_data;
        }

        if (cells[1] == "T")
        {
            TradeData trade_data;
            trade_data.timestamp = timestamp;
            trade_data.symbol = symbol;
            if (!cells[3].empty() || !cells[4].empty() ||
              !cells[5].empty() || !cells[6].empty()) {
                return std::nullopt;
              }

            if (cells[7].empty() || cells[8].empty() ||
              cells[9].empty())
                return std::nullopt;

            if (cells[9] != "B" && cells[9] != "S") {
                return std::nullopt;
            }

            if (!parsePrice(std::string(cells[7]), trade_data.trade_price) ||
              !parseQty(std::string(cells[8]), trade_data.trade_qty))
                return std::nullopt;

            trade_data.trade_side = cells[9][0];
            return trade_data;
        }

        return std::nullopt;
    }

    ParserResult parseCSV()
    {
        size_t invalid_parse_count = 0;
        std::vector<MarketData> data;
        std::string line;
        std::ifstream file(filename_);
        if (!file.is_open())
        {
            std::cerr << "Error opening file\n";
            return {std::nullopt, invalid_parse_count};
        }
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#' || line.find("Timestamp") != std::string::npos) continue;

            std::string_view s(line);

            if (auto record = parse(s); record.has_value())
                data.push_back(std::move(*record));
            else invalid_parse_count++;
        }
        return {std::move(data), invalid_parse_count};
    }
};

#endif //SLIPSTREAM_PARSER_H
