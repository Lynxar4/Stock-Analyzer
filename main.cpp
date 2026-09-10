#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
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

Stock getStock(std::string symbol)
{
	httplib::Client cli("https://www.alphavantage.co"); // Alpha Vantage is the server 
	std::string URL{ "https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=" + symbol + "&apikey=demo" };
	auto res = cli.Get(URL);
	if (res && res->status == 200) {
		json stockData = json::parse(res->body);
		auto data{ stockData["Global Quote"] };
		return { data["01. symbol"].get<std::string>(),
			std::stod(data["05. price"].get<std::string>()) };
	}
	else {
		std::cout << "Request failed.\n";
		return {"Invalid stock", -67};
	}
}

std::string getSymbol()
{
	std::cout << "Enter a stock symbol: ";
	std::string symbol{};
	std::cin >> symbol;
	return symbol;
}

int main()
{
	std::string symbol{ getSymbol() };
	Stock stock{getStock(symbol)};
	stock.printInfo();
}