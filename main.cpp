#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <nlohmann/json.hpp>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

class Stock
{
public:
	Stock() = default;

	Stock(std::string_view name, double price, double peRatio, double dividend, double revenue2026, double debt, double revenue2025)
		:m_name{ name }, m_price{ price }, m_peRatio{ peRatio }, m_dividend{ dividend }, m_revenue2026{ revenue2026 }, m_debt{ debt }, m_revenue2025{revenue2025}
	{}

	void printInfo() const;
	double getRatio() const { return m_peRatio; }
	double getGrowth() const { return revenueGrowth; }

private:
	std::string m_name{};
	double m_price{};
	double m_peRatio{};
	double m_dividend{};
	double m_revenue2026{};
	double m_revenue2025{};
	double m_debt{};
	double revenueGrowth{ (m_revenue2026 - m_revenue2025) / m_revenue2025 * 100 };
};

void Stock::printInfo() const
{
	std::cout << "Name: " << m_name << '\n';
	std::cout << "Price: " << '$' << m_price << '\n';
	std::cout << "P/E Ratio: " << m_peRatio << '\n';
	std::cout << "Dividend: " << m_dividend << "%\n";
	std::cout << "Revenue: " << m_revenue2026 << " billion\n";
	std::cout << "Revenue growth: " << revenueGrowth << "%\n";
	std::cout << "Debt: " << m_debt << " billion\n";
	std::cout << '\n';
}

bool compare(Stock& x, Stock& y)
{
	return x.getRatio() < y.getRatio();
}

void printStockList(const std::vector<Stock>& stocks)
{
	for (const auto& i : stocks)
	{
		i.printInfo();
	}
}

using json = nlohmann::json;

int main()
{
	httplib::Client cli("https://www.alphavantage.co"); // Alpha Vantage is the server 

	auto res = cli.Get("https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=MSFT&apikey=demo");
	if (res && res->status == 200) {
		json data = json::parse(res->body);
		std::cout << data;
	}
	else {
		std::cout << "Request failed.\n";
	}


	std::cout << '\n';
	std::vector<Stock> stocks{};
	Stock Microsoft{ "Microsoft", 487.31, 27.15, 0.75, 331.84, 31.07, 281.7 };
	Stock Apple{ "Apple", 310.34, 35.58, 0.35, 364.36, 84.34, 416.16 };
	Stock Google{ "Google", 344.59, 17.31, 0.26, 229.7, 98.165, 402.84 };
	Stock Nvidia{ "Nvidia", 208.48, 31.93, 0.48, 215.9, 11.41, 130.5 };
	Stock AMD{ "AMD", 456.74, 117.20, 0, 21.889, 4.28, 34.639 };

	stocks.push_back(Microsoft);
	stocks.push_back(Apple);
	stocks.push_back(Google);
	stocks.push_back(Nvidia);
	stocks.push_back(AMD);

	printStockList(stocks);

	std::vector<Stock> sortedStocks{ stocks };
	std::sort(sortedStocks.begin(), sortedStocks.end(), compare);

	std::cout << "Ranking stocks by P/E Ratio\n";
	printStockList(sortedStocks);
}