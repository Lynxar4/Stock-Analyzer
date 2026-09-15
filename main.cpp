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
#include <nlohmann/json.hpp>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

using json = nlohmann::json;

struct Stock
{
	std::string symbol{};
	double price{};
	double EPS{};
	double EPSGrowth{};
	double PE{};
	double PEG{};
	double ROE{};
};

void printStock(const Stock& stock) 
{
	std::cout << "Symbol: " << stock.symbol << '\n';
	std::cout << "Price: " << stock.price << '\n';
	std::cout << "Earnings per share: " << '$' << stock.EPS << '\n';
	std::cout << "Earnings per share growth: " << stock.EPSGrowth << "%\n";
	std::cout << "P/E Ratio: " << stock.PE << '\n';
	std::cout << "PEG Ratio: " << stock.PEG << '\n';
	std::cout << "Return on equity: " << stock.ROE << '\n';
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
	else if (res->status == 429)
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
		return std::nullopt;

	std::optional<json> metricData{ getData(symbol, apiKey, "metric") };
	if (!metricData)
		return std::nullopt;

	std::string stockSymbol{ (*metricData)["symbol"].get<std::string>()};
	double price{ (*quoteData)["c"].get<double>() };
	double EPS{ (*metricData)["metric"]["epsTTM"].get<double>() };
	double EPSGrowth{ (*metricData)["metric"]["epsGrowthTTMYoy"].get<double>() };
	double PERatio{ (*metricData)["metric"]["peTTM"].get<double>() };
	double PEGRatio{PERatio / EPSGrowth};
	double ROE{ (*metricData)["metric"]["roeTTM"].get<double>() };

	return Stock{stockSymbol, price, EPS, EPSGrowth, PERatio, PEGRatio, ROE};
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

std::string_view compareValues(Stock& stock1, Stock& stock2, std::string_view metric)
{
	if (metric == "EPSGrowth")
	{
		if (stock1.EPSGrowth > stock2.EPSGrowth)
			return stock1.symbol;
		else if (stock2.EPSGrowth > stock1.EPSGrowth)
			return stock2.symbol;
		else
			return "TIE";
	}
	else if (metric == "PE")
	{
		if (stock1.PE < stock2.PE)
			return stock1.symbol;
		else if (stock2.PE < stock1.PE)
			return stock2.symbol;
		else
			return "TIE";
	}
	else if (metric == "PEG")
	{
		if (stock1.PEG < stock2.PEG)
			return stock1.symbol;
		else if (stock2.PEG < stock1.PEG)
			return stock2.symbol;
		else
			return "TIE";
	}
	else if (metric == "ROE")
	{
		if (stock1.ROE > stock2.ROE)
			return stock1.symbol;
		else if (stock2.ROE > stock1.ROE)
			return stock2.symbol;
		else
			return "TIE";
	}
	return "INVALID";
}

void compare(Stock& stock1, Stock& stock2)
{
	printStock(stock1);
	printStock(stock2);
	std::cout << std::left << std::setw(15) << "" << std::setw(15) << stock1.symbol << std::setw(15) << stock2.symbol << "Winner\n";
	std::cout << std::left << std::setw(15) << "EPS Growth: " << std::setw(15) << stock1.EPSGrowth << std::setw(15) << stock2.EPSGrowth << compareValues(stock1, stock2, "EPSGrowth") << "\n";
	std::cout << std::left << std::setw(15) << "P/E Ratio: " << std::setw(15) << stock1.PE << std::setw(15) << stock2.PE << compareValues(stock1, stock2, "PE") << '\n';
	std::cout << std::left << std::setw(15) << "PEG Ratio: " << std::setw(15) << stock1.PEG << std::setw(15) << stock2.PEG << compareValues(stock1, stock2, "PEG") << '\n';
	std::cout << std::left << std::setw(15) << "ROE: " << std::setw(15) << stock1.ROE << std::setw(15) << stock2.ROE << compareValues(stock1, stock2, "ROE") << "\n";
}

int main()
{
	auto apiKey{ getApiKey() };
	if (!apiKey)
		return 1;

	std::string symbol1{ getSymbol() };
	std::optional<Stock> stock1{getStock(symbol1, *apiKey)};
	if (!stock1)
		return 1;

	std::string symbol2{ getSymbol() };
	std::optional<Stock> stock2{ getStock(symbol2, *apiKey) };
	if (!stock2)
		return 1;

	compare(*stock1, *stock2);

	return 0;
}