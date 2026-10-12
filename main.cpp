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
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "httplib.h"

constexpr double kMissing{ std::numeric_limits<double>::quiet_NaN()};

using json = nlohmann::json;

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
	std::cout << "Valuation:\n";
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

std::string createURL(std::string symbol, std::string apiKey, std::string function)
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

std::optional<json> getData(std::string symbol, std::string apiKey, std::string function)
{
	httplib::Client cli("https://finnhub.io");
	std::string URL{ createURL(symbol, apiKey, function) };
	auto res = cli.Get(URL);
	if (res && res->status == 200)
	{
		json data = json::parse(res->body);	
		if (function == "quote")
		{
			if (data["c"] == 0)
			{
				std::cout << "You entered an invalid symbol.\n";
				return std::nullopt;
			}
		}
		else if (function == "metric")
		{
			if (data["metric"].empty())
			{
				std::cout << "You entered an invalid symbol.\n";
				return std::nullopt;
			}
		}
		return data;
	}
	else if (res && res->status == 429)
	{
		std::cout << "You reached your rate limit.\n";
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

std::optional<Stock> getStock(std::string symbol, std::string apiKey)
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
		const json metric{ (*metricData)["metric"] };
		Stock stock{};
		stock.symbol = (*metricData)["symbol"].get<std::string>();
		stock.price = (*quoteData)["c"].get<double>();
		stock.revenueGrowth3Y = getMetric(metric, "revenueGrowth3Y");
		stock.epsGrowth3Y = getMetric(metric, "epsGrowth3Y");
		stock.grossMarginTTM = getMetric(metric, "grossMarginTTM");
		stock.operatingMarginTTM = getMetric(metric, "operatingMarginTTM");
		stock.roeTTM = getMetric(metric, "roeTTM");
		stock.roicTTM = getLatestQuarterly(*metricData, "roicTTM");
		stock.peTTM = getMetric(metric, "peTTM");
		stock.pegTTM = getMetric(metric, "pegTTM");

		return stock;
	}
	catch (const json::exception&)
	{
		std::cout << "Missing or invalid data for this stock.\n";
		return std::nullopt;
	}
}

std::string getSymbol()
{
	std::cout << "Enter a stock symbol: ";
	std::string symbol{};
	std::getline(std::cin >> std::ws, symbol);
	std::transform(symbol.begin(), symbol.end(), symbol.begin(),
		[](unsigned char c) { return std::toupper(c); }); 
	return symbol;
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

int main()
{
	std::vector<Stock> stocks{};
	auto apiKey{ getApiKey() };
	if (!apiKey)
	{
		return 1;
	}

	std::cout << "Welcome to stock analyzer, enter some stocks and press -1 to rank them for growth potential.\n";
	while (true)
	{
		std::string symbol{ getSymbol() };
		if (symbol == "-1")
		{
			std::cout << '\n';
			break;
		}
		std::optional<Stock> stock{ getStock(symbol, *apiKey) };
		if (!stock)
		{
			continue;
		}
		stocks.push_back(*stock);
	}
	for (const auto& e : stocks)
	{
		printStock(e);
	}

	return 0;
}