#include <gtest/gtest.h>

#include "lib/parser.h"

#include <fstream>
#include <string>
#include <string_view>
#include <variant>

class ParserTest : public ::testing::Test {
protected:
    static void expectRejected(std::string_view row) {
        EXPECT_FALSE(Parser::parse(row).has_value())
            << "Unexpectedly accepted: " << row;
    }

    static QuoteData expectQuote(std::string_view row) {
        auto result = Parser::parse(row);
        EXPECT_TRUE(result.has_value()) << "Unexpectedly rejected: " << row;
        if (!result.has_value() || !std::holds_alternative<QuoteData>(*result)) {
            ADD_FAILURE() << "Not a quote: " << row;
            return {};
        }
        return std::get<QuoteData>(*result);
    }

    static TradeData expectTrade(std::string_view row) {
        auto result = Parser::parse(row);
        EXPECT_TRUE(result.has_value()) << "Unexpectedly rejected: " << row;
        if (!result.has_value() || !std::holds_alternative<TradeData>(*result)) {
            ADD_FAILURE() << "Not a trade: " << row;
            return {};
        }
        return std::get<TradeData>(*result);
    }
};

// ---------------------------------------------------------------------------
// Valid rows
// ---------------------------------------------------------------------------

TEST_F(ParserTest, ParsesValidQuote) {
    const QuoteData quote = expectQuote("09:30:00.003,Q,SYNTH3,87.37,55,87.49,50,,,");

    EXPECT_EQ(quote.timestamp, "09:30:00.003");
    EXPECT_EQ(quote.symbol, "SYNTH3");
    EXPECT_EQ(quote.bid_price, 873700);
    EXPECT_EQ(quote.bid_qty, 55U);
    EXPECT_EQ(quote.ask_price, 874900);
    EXPECT_EQ(quote.ask_qty, 50U);
}

TEST_F(ParserTest, ParsesValidBuyTrade) {
    const TradeData trade = expectTrade("09:30:00.190,T,SYNTH2,,,,,248.53,65,B");

    EXPECT_EQ(trade.timestamp, "09:30:00.190");
    EXPECT_EQ(trade.symbol, "SYNTH2");
    EXPECT_EQ(trade.trade_price, 2485300);
    EXPECT_EQ(trade.trade_qty, 65U);
    EXPECT_EQ(trade.trade_side, 'B');
}

TEST_F(ParserTest, ParsesValidSellTrade) {
    const TradeData trade = expectTrade("09:30:02.051,T,SYNTH4,,,,,34.22,225,S");

    EXPECT_EQ(trade.trade_price, 342200);
    EXPECT_EQ(trade.trade_qty, 225U);
    EXPECT_EQ(trade.trade_side, 'S');
}

// ---------------------------------------------------------------------------
// Price conversion: decimal text -> fixed point x10,000
// ---------------------------------------------------------------------------

TEST_F(ParserTest, ConvertsPricesExactly) {
    // 101.23 and 34.19 are not exactly representable as doubles; truncating
    // (instead of rounding) the scaled value would give 1012299 / 341899.
    EXPECT_EQ(expectQuote("09:30:00.000,Q,SYNTH1,101.23,1,34.19,1,,,").bid_price, 1012300);
    EXPECT_EQ(expectQuote("09:30:00.000,Q,SYNTH1,101.23,1,34.19,1,,,").ask_price, 341900);

    // No fractional part, one decimal, four decimals, high-priced symbol.
    EXPECT_EQ(expectTrade("09:30:00.000,T,SYNTH5,,,,,1250,3,B").trade_price, 12500000);
    EXPECT_EQ(expectTrade("09:30:00.000,T,SYNTH4,,,,,34.2,3,B").trade_price, 342000);
    EXPECT_EQ(expectTrade("09:30:00.000,T,SYNTH4,,,,,0.0001,3,B").trade_price, 1);
    EXPECT_EQ(expectTrade("09:30:00.000,T,SYNTH5,,,,,1249.95,3,S").trade_price, 12499500);
}

TEST_F(ParserTest, PassesNegativeAndZeroPricesThrough) {
    // The client does not judge the data: semantic validation (price <= 0,
    // crossed quotes) is the server's job, so adversarial CSVs can reach it.
    EXPECT_EQ(expectTrade("09:30:00.000,T,SYNTH1,,,,,-87.37,3,B").trade_price, -873700);
    EXPECT_EQ(expectTrade("09:30:00.000,T,SYNTH1,,,,,0,3,B").trade_price, 0);

    const QuoteData crossed = expectQuote("09:30:00.000,Q,SYNTH1,101.30,1,101.20,1,,,");
    EXPECT_GT(crossed.bid_price, crossed.ask_price);
}

// ---------------------------------------------------------------------------
// Quantity bounds (wire type is uint32)
// ---------------------------------------------------------------------------

TEST_F(ParserTest, AcceptsQuantityAtUint32Max) {
    EXPECT_EQ(expectTrade("09:30:00.000,T,SYNTH1,,,,,1.00,4294967295,B").trade_qty, 4294967295U);
    EXPECT_EQ(expectTrade("09:30:00.000,T,SYNTH1,,,,,1.00,0,B").trade_qty, 0U);
}

TEST_F(ParserTest, RejectsQuantityThatDoesNotFitUint32) {
    expectRejected("09:30:00.000,T,SYNTH1,,,,,1.00,4294967296,B");
    expectRejected("09:30:00.000,T,SYNTH1,,,,,1.00,99999999999999999999999,B");
}

TEST_F(ParserTest, RejectsNegativeQuantity) {
    // stoull("-1") does not throw, it wraps; the parser must catch the sign itself.
    expectRejected("09:30:00.000,T,SYNTH1,,,,,1.00,-1,B");
    expectRejected("09:30:00.000,Q,SYNTH1,1.00,-5,1.01,5,,,");
    expectRejected("09:30:00.000,Q,SYNTH1,1.00,5,1.01,-5,,,");
}

TEST_F(ParserTest, RejectsNonIntegerQuantity) {
    expectRejected("09:30:00.000,T,SYNTH1,,,,,1.00,abc,B");
    expectRejected("09:30:00.000,T,SYNTH1,,,,,1.00,65.5,B");
    expectRejected("09:30:00.000,T,SYNTH1,,,,,1.00,65x,B");
}

// ---------------------------------------------------------------------------
// Symbol (wire field is char[12])
// ---------------------------------------------------------------------------

TEST_F(ParserTest, AcceptsSymbolThatExactlyFillsWireField) {
    EXPECT_EQ(expectQuote("09:30:00.000,Q,ABCDEFGHIJKL,87.37,50,87.40,60,,,").symbol, "ABCDEFGHIJKL");
    EXPECT_EQ(expectTrade("09:30:00.000,T,ABCDEFGHIJKL,,,,,87.37,50,B").symbol, "ABCDEFGHIJKL");
}

TEST_F(ParserTest, RejectsSymbolThatDoesNotFitWireField) {
    expectRejected("09:30:00.000,Q,ABCDEFGHIJKLM,87.37,50,87.40,60,,,");
    expectRejected("09:30:00.000,T,ABCDEFGHIJKLM,,,,,87.37,50,B");
    expectRejected("09:30:00.000,Q,ABCDEFGHIJKLMNOPQRSTUV,87.37,50,87.40,60,,,");
}

TEST_F(ParserTest, RejectsEmptySymbolOrTimestamp) {
    expectRejected("09:30:00.000,Q,,87.37,50,87.40,60,,,");
    expectRejected("09:30:00.000,T,,,,,,87.37,50,B");
    expectRejected(",Q,SYNTH1,87.37,50,87.40,60,,,");
    expectRejected(",T,SYNTH1,,,,,87.37,50,B");
}

// ---------------------------------------------------------------------------
// Row shape: column count and record type
// ---------------------------------------------------------------------------

TEST_F(ParserTest, RejectsWrongColumnCount) {
    expectRejected("");
    expectRejected("09:30:00.003");
    expectRejected("09:30:00.003,Q,SYNTH3");                          // truncated after symbol
    expectRejected("09:30:00.003,Q,SYNTH3,87.37");                    // truncated after bid
    expectRejected("09:30:00.003,Q,SYNTH3,87.37,55,87.49,50,,");      // 9 columns
    expectRejected("09:30:00.003,Q,SYNTH3,87.37,55,87.49,50,,,,");    // 11 columns
    expectRejected("09:30:00.190,T,SYNTH2,,,,,248.53,65");            // trade, 9 columns
    expectRejected("09:30:00.190,T,SYNTH2,,,,,248.53,65,B,");         // trade, 11 columns
}

TEST_F(ParserTest, RejectsRowThatIsNeitherQuoteNorTrade) {
    expectRejected("09:30:00.003,X,SYNTH3,87.37,55,87.49,50,,,");
    expectRejected("09:30:00.003,,SYNTH3,87.37,55,87.49,50,,,");
    expectRejected("09:30:00.003,q,SYNTH3,87.37,55,87.49,50,,,");     // case sensitive
    expectRejected("09:30:00.190,t,SYNTH2,,,,,248.53,65,B");
    expectRejected("09:30:00.003,Quote,SYNTH3,87.37,55,87.49,50,,,"); // must be exactly "Q"
    expectRejected("09:30:00.190,Trade,SYNTH2,,,,,248.53,65,B");
    expectRejected("09:30:00.003,QT,SYNTH3,87.37,55,87.49,50,,,");
}

TEST_F(ParserTest, RejectsHeaderAndCommentLines) {
    // parseCSV skips these before calling parse(); parse() must still refuse them.
    expectRejected("Timestamp,Type,Symbol,BidPrice,BidQty,AskPrice,AskQty,Price,Qty,Aggressor");
    expectRejected("# Sample Market Data CSV");
    expectRejected("#");
}

// ---------------------------------------------------------------------------
// Invalid quotes
// ---------------------------------------------------------------------------

TEST_F(ParserTest, RejectsQuoteWithMissingField) {
    expectRejected("09:30:00.003,Q,SYNTH3,,55,87.49,50,,,");      // bid price
    expectRejected("09:30:00.003,Q,SYNTH3,87.37,,87.49,50,,,");   // bid qty
    expectRejected("09:30:00.003,Q,SYNTH3,87.37,55,,50,,,");      // ask price
    expectRejected("09:30:00.003,Q,SYNTH3,87.37,55,87.49,,,,");   // ask qty
    expectRejected("09:30:00.003,Q,SYNTH3,,,,,,,");               // all of them
}

TEST_F(ParserTest, RejectsQuoteWithTradeFieldsFilled) {
    expectRejected("09:30:00.003,Q,SYNTH3,87.37,55,87.49,50,87.40,,");  // price
    expectRejected("09:30:00.003,Q,SYNTH3,87.37,55,87.49,50,,10,");     // qty
    expectRejected("09:30:00.003,Q,SYNTH3,87.37,55,87.49,50,,,B");      // aggressor
    expectRejected("09:30:00.003,Q,SYNTH3,87.37,55,87.49,50,87.40,10,B");
}

TEST_F(ParserTest, RejectsQuoteWithNonNumericPrice) {
    expectRejected("09:30:00.003,Q,SYNTH3,abc,55,87.49,50,,,");
    expectRejected("09:30:00.003,Q,SYNTH3,87.37,55,abc,50,,,");
    expectRejected("09:30:00.003,Q,SYNTH3,87.37abc,55,87.49,50,,,");  // trailing junk
    expectRejected("09:30:00.003,Q,SYNTH3,87.37,55,87.4.9,50,,,");
    expectRejected("09:30:00.003,Q,SYNTH3,nan,55,87.49,50,,,");
    expectRejected("09:30:00.003,Q,SYNTH3,87.37,55,inf,50,,,");
}

// ---------------------------------------------------------------------------
// Invalid trades
// ---------------------------------------------------------------------------

TEST_F(ParserTest, RejectsTradeWithMissingField) {
    expectRejected("09:30:00.190,T,SYNTH2,,,,,,65,B");        // price
    expectRejected("09:30:00.190,T,SYNTH2,,,,,248.53,,B");    // qty
    expectRejected("09:30:00.190,T,SYNTH2,,,,,248.53,65,");   // aggressor
    expectRejected("09:30:00.190,T,SYNTH2,,,,,,,");           // all of them
}

TEST_F(ParserTest, RejectsTradeWithQuoteFieldsFilled) {
    expectRejected("09:30:00.190,T,SYNTH2,248.50,,,,248.53,65,B");  // bid price
    expectRejected("09:30:00.190,T,SYNTH2,,100,,,248.53,65,B");     // bid qty
    expectRejected("09:30:00.190,T,SYNTH2,,,248.55,,248.53,65,B");  // ask price
    expectRejected("09:30:00.190,T,SYNTH2,,,,100,248.53,65,B");     // ask qty
    expectRejected("09:30:00.190,T,SYNTH2,248.50,100,248.55,100,248.53,65,B");
}

TEST_F(ParserTest, RejectsTradeWithInvalidAggressor) {
    expectRejected("09:30:00.190,T,SYNTH2,,,,,248.53,65,X");
    expectRejected("09:30:00.190,T,SYNTH2,,,,,248.53,65,b");    // case sensitive
    expectRejected("09:30:00.190,T,SYNTH2,,,,,248.53,65,BUY");  // must be exactly one char
    expectRejected("09:30:00.190,T,SYNTH2,,,,,248.53,65,BS");
    expectRejected("09:30:00.190,T,SYNTH2,,,,,248.53,65, ");
}

TEST_F(ParserTest, RejectsTradeWithNonNumericPrice) {
    expectRejected("09:30:00.190,T,SYNTH2,,,,,abc,65,B");
    expectRejected("09:30:00.190,T,SYNTH2,,,,,248.53abc,65,B");
    expectRejected("09:30:00.190,T,SYNTH2,,,,,nan,65,B");
    expectRejected("09:30:00.190,T,SYNTH2,,,,,-inf,65,B");
}

// ---------------------------------------------------------------------------
// parseCSV: whole-file behaviour
// ---------------------------------------------------------------------------

class ParseCsvTest : public ::testing::Test {
protected:
    static std::string writeTempCsv(const std::string& name, const std::string& contents) {
        const std::string path = ::testing::TempDir() + name;
        std::ofstream out(path, std::ios::trunc);
        out << contents;
        return path;
    }
};

TEST_F(ParseCsvTest, ReportsMissingFile) {
    Parser parser(::testing::TempDir() + "slipstream_does_not_exist.csv");

    const ParserResult result = parser.parseCSV();

    EXPECT_FALSE(result.data.has_value());
}

TEST_F(ParseCsvTest, SkipsPreambleHeaderAndBlankLines) {
    const std::string path = writeTempCsv("slipstream_preamble.csv",
        "# Sample Market Data CSV\n"
        "# Format: Timestamp,Type,Symbol,BidPrice,BidQty,AskPrice,AskQty (for quotes)\n"
        "#\n"
        "\n"
        "Timestamp,Type,Symbol,BidPrice,BidQty,AskPrice,AskQty,Price,Qty,Aggressor\n"
        "09:30:00.003,Q,SYNTH3,87.37,55,87.49,50,,,\n"
        "09:30:00.190,T,SYNTH2,,,,,248.53,65,B\n");

    const ParserResult result = Parser(path).parseCSV();

    ASSERT_TRUE(result.data.has_value());
    EXPECT_EQ(result.invalid_parse_count, 0U);
    ASSERT_EQ(result.data->size(), 2U);
    EXPECT_TRUE(std::holds_alternative<QuoteData>((*result.data)[0]));
    EXPECT_TRUE(std::holds_alternative<TradeData>((*result.data)[1]));
}

TEST_F(ParseCsvTest, CountsInvalidRowsAndKeepsValidOnesInOrder) {
    const std::string path = writeTempCsv("slipstream_mixed.csv",
        "Timestamp,Type,Symbol,BidPrice,BidQty,AskPrice,AskQty,Price,Qty,Aggressor\n"
        "09:30:00.003,Q,SYNTH3,87.37,55,87.49,50,,,\n"
        "09:30:00.100,Q,SYNTH3\n"                          // truncated
        "09:30:00.190,T,SYNTH2,,,,,248.53,65,B\n"
        "09:30:00.200,X,SYNTH2,,,,,248.53,65,B\n"          // unknown type
        "09:30:00.215,Q,SYNTH1,101.23,175,101.25,150,,,\n");

    const ParserResult result = Parser(path).parseCSV();

    ASSERT_TRUE(result.data.has_value());
    EXPECT_EQ(result.invalid_parse_count, 2U);
    ASSERT_EQ(result.data->size(), 3U);
    EXPECT_EQ(std::get<QuoteData>((*result.data)[0]).symbol, "SYNTH3");
    EXPECT_EQ(std::get<TradeData>((*result.data)[1]).symbol, "SYNTH2");
    EXPECT_EQ(std::get<QuoteData>((*result.data)[2]).bid_price, 1012300);
}

TEST_F(ParseCsvTest, EmptyFileYieldsNoRecords) {
    const std::string path = writeTempCsv("slipstream_empty.csv", "");

    const ParserResult result = Parser(path).parseCSV();

    ASSERT_TRUE(result.data.has_value());
    EXPECT_TRUE(result.data->empty());
    EXPECT_EQ(result.invalid_parse_count, 0U);
}
