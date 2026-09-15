#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <optional>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

using json = nlohmann::json;

class Stock
{
public:
	Stock() = default;

	Stock(std::string_view symbol, double price, double revenue, double earningsGrowth, double EPS, double PE, double PEG, double ROE)
		:m_symbol{ symbol }, m_price{price}, m_revenue{revenue}, m_earningsGrowth{earningsGrowth}, m_EPS{ EPS }, m_PE{PE}, m_PEG{PEG}, m_ROE{ROE}
	{}

	void printInfo() const;

private:
	std::string m_symbol{};
	double m_price{};
	double m_revenue{};
	double m_earningsGrowth{};
	double m_EPS{};
	double m_PE{};
	double m_PEG{};
	double m_ROE{};

};

void Stock::printInfo() const
{
	std::cout << "Symbol: " << m_symbol << '\n';
	std::cout << "Price: " << m_price << '\n';
	std::cout << "Revenue: " << m_revenue << '\n';
	std::cout << "QuarterlyEarningsGrowthYOY: " << m_earningsGrowth << '\n';
	std::cout << "Earnings per share: " << m_EPS << '\n';
	std::cout << "P/E Ratio: " << m_PE << '\n';
	std::cout << "PEG Ratio: " << m_PEG << '\n';
	std::cout << "Return on equity: " << m_ROE << '\n';
}

void printStockList(const std::vector<Stock>& stocks) 
{
	for (const auto& i : stocks)
	{
		i.printInfo();
	}
}

std::optional<json> getData(std::string symbol, std::string apiKey, std::string function)
{
	httplib::Client cli("https://www.alphavantage.co");
	std::string URL{ "https://www.alphavantage.co/query?function=" + function + "&symbol=" + symbol + "&apikey=" + apiKey }; 
	auto res = cli.Get(URL);
	if (res && res->status == 200)
	{
		json data = json::parse(res->body);	

		if (data.contains("Information"))
		{
			std::cout << "Alpha Vantage reached its rate limit.\n";
			return std::nullopt;
		}
		if (function == "GLOBAL_QUOTE")
		{
			if (data["Global Quote"].empty())
			{
				std::cout << "You entered an invalid symbol.\n";
				return std::nullopt;
			}
		}
		else if (function == "OVERVIEW")
		{
			if (data.empty())
			{
				std::cout << "You entered an invalid symbol.\n";
				return std::nullopt;
			}
		}
		return data;
	}
	else
	{
		std::cout << "Request failed.\n";
		return std::nullopt;
	}
}

std::optional<Stock> getStock(std::string symbol, std::string apiKey)
{
	std::optional<json> quoteData{ getData(symbol, apiKey, "GLOBAL_QUOTE") }; // Get price data
	if (!quoteData)
		return std::nullopt;

	std::this_thread::sleep_for(std::chrono::seconds(1)); // 1 second gap to avoid rate limit response

	std::optional<json> metricData{ getData(symbol, apiKey, "OVERVIEW") }; 
	if (!metricData)
		return std::nullopt;

	std::string symbolUppercase{ (*metricData)["Symbol"].get<std::string>() };
	double price{ std::stod((*quoteData)["Global Quote"]["05. price"].get<std::string>()) };
	double revenue{ std::stod((*metricData)["RevenueTTM"].get<std::string>()) };
	double earningGrowth{ std::stod((*metricData)["QuarterlyEarningsGrowthYOY"].get<std::string>()) };
	double EPS{ std::stod((*metricData)["EPS"].get<std::string>()) };
	double PE{ std::stod((*metricData)["PERatio"].get<std::string>()) };
	double PEG{ std::stod((*metricData)["PEGRatio"].get<std::string>()) };
	double ROE{ std::stod((*metricData)["ReturnOnEquityTTM"].get<std::string>()) };

	return Stock{symbolUppercase, price, revenue, earningGrowth, EPS, PE, PEG, ROE};
}

std::string getSymbol()
{
	std::cout << "Enter a stock symbol: ";
	std::string symbol{};
	std::getline(std::cin >> std::ws, symbol);
	return symbol;
}

std::optional<std::string> getApiKey()
{
	const char* apiKey{ std::getenv("ALPHA_VANTAGE_API_KEY") };
	if (!apiKey)
	{
		std::cout << "API key could not be found";
		return std::nullopt;
	}
	
	return std::string{ apiKey };
}

int main()
{
	std::string symbol{ getSymbol() };
	auto apiKey{getApiKey()};
	if (!apiKey)
		return 1;

	std::optional<Stock> stock{getStock(symbol, *apiKey)};
	if (!stock)
		return 1;
	stock->printInfo();

	return 0;
}