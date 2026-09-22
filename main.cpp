#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <optional>
#include <iomanip>
#include <stdexcept>
#include <nlohmann/json.hpp>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

using json = nlohmann::json;

struct Stock
{
	std::string symbol{};
	double price{};
	double EPS{};
	double ROE{};
	double PE{};
	double EPSGrowth3Y{};
	double PEG3Y{};
	double revenueGrowth3Y{};
};

void printStock(const Stock& stock) 
{
	std::cout << "Symbol: " << stock.symbol << '\n';
	std::cout << "Price: $" << stock.price << '\n';
	std::cout << "Earnings per share: $" << stock.EPS << '\n';
	std::cout << "P/E Ratio: " << stock.PE << '\n';
	std::cout << "Return on equity: " << stock.ROE << "%\n";
	std::cout << "Growth metrics (3 year data):\n";
	std::cout << "EPS Growth: " << stock.EPSGrowth3Y << "%\n";
	std::cout << "PEG Ratio: " << stock.PEG3Y << '\n';
	std::cout << "Revenue Growth: " << stock.revenueGrowth3Y << "%\n";
	std::cout << '\n';
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

	std::string stockSymbol{};
	double price{};
	double EPS{};
	double ROE{};
	double PE{};
	double EPSGrowth3Y{};
	double PEG3Y{};
	double revenueGrowth3Y{};
	try
	{
		stockSymbol = (*metricData)["symbol"].get<std::string>();
		price = (*quoteData)["c"].get<double>();
		EPS = (*metricData)["metric"]["epsTTM"].get<double>();
		ROE = (*metricData)["metric"]["roeTTM"].get<double>();
		PE = (*metricData)["metric"]["peTTM"].get<double>();
		EPSGrowth3Y = (*metricData)["metric"]["epsGrowth3Y"].get<double>();
		if (EPSGrowth3Y > 0)
		{
			PEG3Y = PE / EPSGrowth3Y;
		}
		else
			PEG3Y = 0;
		revenueGrowth3Y = (*metricData)["metric"]["revenueGrowth3Y"].get<double>();
	}
	catch (const json::exception&)
	{
		std::cout << "Missing or invalid data for this stock.\n";
		return std::nullopt;
	}

	return Stock{.symbol = stockSymbol, .price = price, .EPS = EPS, .ROE = ROE, .PE = PE, .EPSGrowth3Y = EPSGrowth3Y, .PEG3Y = PEG3Y, .revenueGrowth3Y = revenueGrowth3Y};
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