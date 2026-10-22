#define _CRT_SECURE_NO_WARNINGS
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <optional>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <fstream>
#include <nlohmann/json.hpp>
#include "httplib.h"

using json = nlohmann::json;

constexpr double kMissing{ std::numeric_limits<double>::quiet_NaN()};
constexpr double growthWeight{0.4};
constexpr double profitabilityWeight{ 0.3 };
constexpr double valuationWeight{ 0.3 };
constexpr int growthMinimumMetrics{2};
constexpr int protfitabilityMinimumMetrics{3};
constexpr int valuationMinimumMetrics{ 2 };
constexpr bool debug{ false };

constexpr bool isValidWeights()
{
	double distance{ 1 - (growthWeight + profitabilityWeight + valuationWeight) };
	return distance < 0 ? -distance < 1e-9 : distance < 1e-9;
}

static_assert(isValidWeights());


struct Stock
{
	std::string symbol{};
	double price{};

	// Growth 
	double revenueGrowth3Y{};
	double epsGrowth3Y{};

	// Profitability 
	double grossMarginTTM{};
	double operatingMarginTTM{};
	double roeTTM{};
	double roicTTM{};

	// Valuation 
	double peTTM{};
	double pegTTM{};
};

struct StockScore
{
	std::string symbol{};
	double gScore{};
	double pScore{};
	double vScore{};
	double finalScore{};
	double finalZScore{};
};

void printStock(const Stock& s) 
{
	std::cout << "Symbol: " << s.symbol << '\n';
	std::cout << "Price: $" << s.price << '\n';
	std::cout << "Growth (3Y):\n";
	std::cout << " Revenue: " << s.revenueGrowth3Y << "%\n";
	std::cout << " EPS: " << s.epsGrowth3Y << "%\n";
	std::cout << "Profitability (TTM):\n";
	std::cout << " Gross margin: " << s.grossMarginTTM << "%\n";
	std::cout << " Operating margin: " << s.operatingMarginTTM << "%\n";
	std::cout << " ROE: " << s.roeTTM << "%\n";
	std::cout << " ROIC: " << s.roicTTM << "%\n";
	std::cout << "Valuation (TTM):\n";
	std::cout << " P/E: " << s.peTTM << '\n';
	std::cout << " PEG: " << s.pegTTM << '\n';
	std::cout << '\n';
}

double getMetric(const json& metric, const std::string& key)
{
	auto it{ metric.find(key) };
	if (it == metric.end() || !it->is_number())
	{
		return kMissing;
	}
	return it->get<double>();
}

std::string createURL(const std::string& symbol, const std::string& apiKey, std::string_view function)
{
	if (function == "quote")
	{
		return "https://finnhub.io/api/v1/quote?symbol=" + symbol + "&token=" + apiKey;
	}
	else if (function == "metric")
	{
		return "https://finnhub.io/api/v1/stock/metric?symbol=" + symbol + "&metric=all&token=" + apiKey;
	}
	else
	{
		throw std::runtime_error("Function was not valid"); 
	}
}

std::optional<json> getData(const std::string& symbol, const std::string& apiKey, std::string_view function)
{
	httplib::Client cli("https://finnhub.io");
	std::string URL{ createURL(symbol, apiKey, function) };
	auto res = cli.Get(URL);
	if (res && res->status == 200)
	{
		json data = json::parse(res->body, nullptr, false);	
		if (data.is_discarded())
		{
			std::cerr << "Json parse error for " << symbol << " Status: " << res->status << '\n';
			return std::nullopt;
		}
		if (function == "quote")
		{
			if (data["c"] == 0)
			{
				std::cout << "Invalid symbol: " << symbol << '\n';
				return std::nullopt;
			}
		}
		else if (function == "metric")
		{
			if (data["metric"].empty())
			{
				std::cout << "Invalid symbol: " << symbol << '\n';
				return std::nullopt;
			}
		}
		return data;
	}
	else if (res && res->status == 429)
	{
		std::cout << "You reached your rate limit.\n";
		return std::nullopt;
	}
	else
	{
		std::cout << "Request failed.\n";
		return std::nullopt;
	}
}

double getLatestQuarterly(const json& data, const std::string& key)
{
	try
	{
		const json& list{ data.at("series").at("quarterly").at(key) }; // JSON array of {period, v} entries for this metric
		return list.at(0).at("v").get<double>() * 100.0;
	}
	catch (const json::exception&)
	{
		return kMissing;
	}
}

double checkPE(double ratio)
{
	if (ratio <= 0)
	{
		return kMissing;
	}
	return ratio;
}

std::optional<Stock> getStock(const std::string& symbol, const std::string& apiKey)
{
	std::optional<json> quoteData{ getData(symbol, apiKey, "quote") }; // Get price data
	if (!quoteData)
	{
		return std::nullopt;
	}

	std::optional<json> metricData{ getData(symbol, apiKey, "metric") };
	if (!metricData)
	{
		return std::nullopt;
	}

	try
	{
		const json& metric{ (*metricData).at("metric")};
		Stock stock{};
		stock.symbol = symbol;
		stock.price = (*quoteData)["c"].get<double>();
		stock.revenueGrowth3Y = getMetric(metric, "revenueGrowth3Y");
		stock.epsGrowth3Y = getMetric(metric, "epsGrowth3Y");
		stock.grossMarginTTM = getMetric(metric, "grossMarginTTM");
		stock.operatingMarginTTM = getMetric(metric, "operatingMarginTTM");
		stock.roeTTM = getMetric(metric, "roeTTM");
		stock.roicTTM = getLatestQuarterly(*metricData, "roicTTM");
		stock.peTTM = checkPE(getMetric(metric, "peTTM"));
		stock.pegTTM = checkPE(getMetric(metric, "pegTTM"));

		return stock;
	}
	catch (const json::exception&)
	{
		std::cout << "Missing or invalid data for this stock.\n";
		return std::nullopt;
	}
}

std::string toUpper(std::string_view string)
{
	std::string result{ string };
	std::transform(result.begin(), result.end(), result.begin(),
		[](unsigned char c) {return std::toupper(c); });
	return result;
}

std::string getSymbol()
{
	std::cout << "Enter a stock symbol: ";
	std::string symbol{};
	std::getline(std::cin >> std::ws, symbol);
	return toUpper(symbol);
}

std::optional<std::string> getApiKey()
{
	const char* apiKey{ std::getenv("FINNHUB_API_KEY") };
	if (!apiKey)
	{
		std::cout << "API key could not be found";
		return std::nullopt;
	}
	
	return std::string{ apiKey };
}

double getPercentileForMetric(const Stock& stock, const std::vector<Stock>& stocks, double Stock::*metric, bool higherIsBetter)
{
	double beaten{};
	double counter{};
		for (std::size_t i{ 0 }; i < stocks.size(); ++i)
		{
			if (std::isnan(stocks[i].*metric) || std::isnan(stock.*metric))
			{
				continue;
			}
			if (stock.symbol == stocks[i].symbol)
			{
				continue;
			}

			if (higherIsBetter)
			{
				if (stock.*metric > stocks[i].*metric)
				{
					++beaten;
				}
			}
			else
			{
				if (stock.*metric < stocks[i].*metric)
				{
					++beaten;
				}
			}
			++counter;
		}

		if (counter == 0)
		{
			return kMissing;
		}

		return (beaten / counter * 100);

}

double getPercentileAverage(const std::vector<double>& percentiles, int minimumPercentiles)
{
	double sum{};
	double count{};
	for (const auto& e : percentiles)
	{
		if (std::isnan(e))
		{
			continue;
		}
		sum += e;
		++count;
	}
	if (count < minimumPercentiles)
	{
		return kMissing;
	}
	return sum / count;
}

double growthScore(const Stock& stock, const std::vector<Stock>& stocks)
{
	double revenueGrowthPercentile{ getPercentileForMetric(stock, stocks, &Stock::revenueGrowth3Y, true) };
	double epsGrowthPercentile{ getPercentileForMetric(stock, stocks, &Stock::epsGrowth3Y, true) };
	return getPercentileAverage({revenueGrowthPercentile, epsGrowthPercentile}, growthMinimumMetrics);
}

double profitabilityScore(const Stock& stock, const std::vector<Stock>& stocks)
{
	double grossMarginPercentile{ getPercentileForMetric(stock, stocks, &Stock::grossMarginTTM, true) };
	double operatingMarginPercentile{ getPercentileForMetric(stock, stocks, &Stock::operatingMarginTTM, true) };
	double roePercentile{ getPercentileForMetric(stock, stocks, &Stock::roeTTM, true) };
	double roicPercentile{ getPercentileForMetric(stock, stocks, &Stock::roicTTM, true) };
	return getPercentileAverage({ grossMarginPercentile, operatingMarginPercentile, roePercentile, roicPercentile }, protfitabilityMinimumMetrics);
}

double valuationScore(const Stock& stock, const std::vector<Stock>& stocks)
{
	double pePercentile{ getPercentileForMetric(stock, stocks, &Stock::peTTM, false) };
	double pegPercentile{ getPercentileForMetric(stock, stocks, &Stock::pegTTM, false) };
	return getPercentileAverage({ pePercentile, pegPercentile }, valuationMinimumMetrics);
}

double getTotalScore(double gScore, double pScore, double vScore)
{
	if (std::isnan(gScore) || std::isnan(pScore) || std::isnan(vScore))
	{
		return kMissing;
	}
	return gScore * growthWeight + pScore * profitabilityWeight + vScore * valuationWeight;
}

void printMetric(double score)
{
	if (std::isnan(score))
	{
		std::cout << "N/A";
	}
	else
	{
		std::cout << score;
	}
	std::cout << '\n';
}

void printScores(const std::vector<StockScore>& scores)
{
	for (const auto& e : scores)
	{
		if (debug)
		{
			std::cout << e.symbol << " Growth Score: ";
			printMetric(e.gScore);
			std::cout << e.symbol << " Profitability Score: ";
			printMetric(e.pScore);
			std::cout << e.symbol << " Valuation Score: ";
			printMetric(e.vScore);
		}
		std::cout << e.symbol << " Final Score Percentile: ";
		printMetric(e.finalScore);
		std::cout << e.symbol << " Final Z score: ";
		printMetric(e.finalZScore);
		std::cout << '\n';
	}
}

bool isDuplicate(const std::vector<Stock>& stocks, std::string_view symbol)
{
	return std::any_of(stocks.begin(), stocks.end(),
		[symbol](const Stock& s)
		{
			return s.symbol == symbol;
		});
}

double getMean(const std::vector<Stock>& stocks, double Stock::* metric)
{
	double total{};
	double count{};
	for (const auto& e : stocks)
	{
		if (std::isnan(e.*metric))
		{
			continue;
		}
		++count;
		total += e.*metric;
	}

	if (count == 0)
	{
		return kMissing;
	}

	return total / count;
}

double getStandardDeviation(const std::vector<Stock>& stocks, double Stock::* metric, double mean)
{
	double total{};
	int count{};
	for (const auto& e : stocks)
	{
		double deviation{ e.*metric - mean };
		if (std::isnan(e.*metric))
		{
			continue;
		}
		++count;
		double squaredDeviation{ deviation * deviation };
		total += squaredDeviation;
	}
	if (count == 0)
	{
		return kMissing;
	}
	double variance{ total / count };
	return std::sqrt(variance);
}

double getZScore(const Stock& stock, double Stock::*metric, double mean, double standardDeviation)
{
	if (standardDeviation < 1e-9 || std::isnan(standardDeviation) || std::isnan(stock.*metric))
	{
		return kMissing;
	}
	return (stock.*metric - mean) / standardDeviation;
}

double getZScoreForMetric(const Stock& stock, const std::vector<Stock>& stocks, double Stock::* metric)
{
	double mean{ getMean(stocks, metric) };
	double standardDeviation{ getStandardDeviation(stocks, metric, mean) };
	double zScore{ getZScore(stock, metric, mean, standardDeviation) };
	return zScore;
}

double zScoreAverage(const std::vector<double>& zScores, int minimumAmount)
{
	double sum{};
	double count{};
	for (const auto& e : zScores)
	{
		if (std::isnan(e))
		{
			continue;
		}
		sum += e;
		++count;
	}
	if (count < minimumAmount)
	{
		return kMissing;
	}
	return sum / count;
}

double zScoreGrowthAverage(const Stock& stock, const std::vector<Stock>& stocks)
{
	double revenueGrowthZ{ getZScoreForMetric(stock, stocks, &Stock::revenueGrowth3Y) };
	double epsGrowthZ{ getZScoreForMetric(stock, stocks, &Stock::epsGrowth3Y) };
	return zScoreAverage({ revenueGrowthZ, epsGrowthZ }, growthMinimumMetrics);
}

double zScoreProfitabilityAverage(const Stock& stock, const std::vector<Stock>& stocks)
{
	double grossMarginZ{ getZScoreForMetric(stock, stocks, &Stock::grossMarginTTM) };
	double operatingMarginZ{ getZScoreForMetric(stock, stocks, &Stock::operatingMarginTTM) };
	double roeZ{ getZScoreForMetric(stock, stocks, &Stock::roeTTM) };
	double roicZ{ getZScoreForMetric(stock, stocks, &Stock::roicTTM) };
	return zScoreAverage({ grossMarginZ, operatingMarginZ, roeZ, roicZ }, protfitabilityMinimumMetrics);
}

double zScoreValuationAverage(const Stock& stock, const std::vector<Stock>& stocks)
{
	double peZ{getZScoreForMetric(stock, stocks, &Stock::peTTM) * -1}; 
	double pegZ{ getZScoreForMetric(stock, stocks, &Stock::pegTTM) * -1};
	return zScoreAverage({ peZ, pegZ }, valuationMinimumMetrics);
}

double getfinalZScore(double zGrowthAverage, double zProfitabilityAverage, double zValuationAverage)
{
	if (std::isnan(zGrowthAverage) || std::isnan(zProfitabilityAverage) || std::isnan(zValuationAverage))
	{
		return kMissing;
	}
	return zGrowthAverage * growthWeight + zProfitabilityAverage * profitabilityWeight + zValuationAverage * valuationWeight;
}

void displayRanking(const std::vector<Stock>& stocks)
{
	for (const auto& e : stocks)
	{
		printStock(e);
	}
	std::cout << '\n';
	std::vector<StockScore> scores{};
	for (const auto& s : stocks)
	{
		const double gScore{ growthScore(s, stocks) };
		const double pScore{ profitabilityScore(s, stocks) };
		const double vScore{ valuationScore(s, stocks) };
		const double finalScore{ getTotalScore(gScore, pScore, vScore) };

		const double zGrowthAverage{ zScoreGrowthAverage(s, stocks) };
		const double zProfitabilityAverage{ zScoreProfitabilityAverage(s, stocks) };
		const double zValuationAverage{ zScoreValuationAverage(s, stocks) };
		const double finalZScore{ getfinalZScore(zGrowthAverage, zProfitabilityAverage, zValuationAverage) };

		scores.push_back({ s.symbol, gScore, pScore, vScore, finalScore, finalZScore });
	}

	auto it = std::partition(scores.begin(), scores.end(),
		[](const StockScore& score)
		{
			return !std::isnan(score.finalScore);
		}
	);

	std::sort(scores.begin(), it,
		[](const StockScore& a, const StockScore& b)
		{
			return a.finalScore > b.finalScore;
		});

	printScores(scores);
}

void manualStockLoop(std::vector<Stock>& stocks, const std::string& apiKey)
{
	while (true)
	{
		std::string symbol{ getSymbol() };
		if (isDuplicate(stocks, symbol))
		{
			std::cout << "You already entered this stock.\n\n";
			continue;
		}
		if (symbol == "-1")
		{
			break;
		}
		std::optional<Stock> stock{ getStock(symbol, apiKey) };
		if (!stock)
		{
			continue;
		}
		stocks.push_back(*stock);
	}
}

void insertStocksFromFile(std::vector<Stock>& stocks, const std::string& apiKey)
{
	std::cout << "Type the file name: ";
	std::string input{};
	std::getline(std::cin >> std::ws, input);
	std::ifstream inf{ input + ".txt" };
	if (!inf)
	{
		std::cerr << "Couldn't open the file " << input << " for reading.\n";
		return;
	}

	std::string strInput{};
	while (inf >> strInput)
	{
		strInput = toUpper(strInput);
		if (isDuplicate(stocks, strInput))
		{
			std::cout << "Skipping duplicate: " << strInput << '\n';
			continue;
		}
		std::optional<Stock> stock{ getStock(strInput, apiKey) };
		if (!stock)
		{
			continue;
		}
		stocks.push_back(*stock);
	}
	std::cout << '\n';
}

int getValidChoice()
{
	int num{};
	while (true)
	{
		std::cin >> num;
		if (!std::cin)
		{
			std::cout << "That was not a valid input.\n";
			std::cin.clear();
			std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			continue;
		}
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Clear extraneous input in the stream
		return num;
	}
}

int main()
{
	std::vector<Stock> stocks{};																																																

	auto apiKey{ getApiKey() };
	if (!apiKey)
	{
		return 1;
	}

	std::cout << "Welcome to stock analyzer, do you want to input stocks manually or insert a file?\n";
	std::cout << "1: Type manually\n";
	std::cout << "2: Insert a file\n";
	int choice{getValidChoice()};
	switch (choice)
	{
	case 1:
		manualStockLoop(stocks, *apiKey);
		break;
	case 2:
		insertStocksFromFile(stocks, *apiKey);
		break;
	default:
		std::cout << "That was not a valid choice.\n";
		return 1;
	}

	if (stocks.empty())
	{
		std::cout << "You didn't enter any stocks. Ending program now.\n";
		return 0;
	}

	displayRanking(stocks);

	return 0;
}