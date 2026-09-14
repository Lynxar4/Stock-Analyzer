#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <optional>
#include <nlohmann/json.hpp>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

using json = nlohmann::json;

class Stock
{
public:
	Stock() = default;

	Stock(std::string_view symbol, double price)
		:m_symbol{ symbol }, m_price{price}
	{}

	void printInfo() const;

private:
	std::string m_symbol{};
	double m_price{};
};

void Stock::printInfo() const
{
	std::cout << "Symbol: " << m_symbol << '\n';
	std::cout << "Price: " << m_price << '\n';
}

void printStockList(const std::vector<Stock>& stocks) 
{
	for (const auto& i : stocks)
	{
		i.printInfo();
	}
}

std::optional<Stock> getStock(std::string symbol)
{
	auto apiKey{ std::getenv("ALPHA_VANTAGE_API_KEY") };
	if (!apiKey)
	{
		std::cout << "API key could not be found";
		return std::nullopt;
	}
	std::string apiKeyString{ apiKey };

	httplib::Client cli("https://www.alphavantage.co"); // Alpha Vantage is the server 
	std::string URL{ "https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=" + symbol + "&apikey=" + apiKeyString };
	auto res = cli.Get(URL);
	if (res && res->status == 200) {
		json stockData = json::parse(res->body);
		if (stockData.contains("Global Quote") && stockData["Global Quote"].empty())
		{
			std::cout << "You didn't enter a valid symbol buddy.\n";
			return std::nullopt;
		}
		auto data{ stockData["Global Quote"] };
		return Stock { data["01. symbol"].get<std::string>(),
			std::stod(data["05. price"].get<std::string>()) };
	}
	else {
		std::cout << "Request failed.\n";
		return std::nullopt;
	}
}

std::string getSymbol()
{
	std::cout << "Enter a stock symbol: ";
	std::string symbol{};
	std::getline(std::cin >> std::ws, symbol);
	return symbol;
}

int main()
{
	std::string symbol{ getSymbol() };
	std::optional<Stock> stock{getStock(symbol)};
	if (stock)
	{
		stock->printInfo();
	}
}