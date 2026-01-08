#include <iostream>
#include <string>
using namespace std;

class Product {
private:
    int id;
    string name;
    double price;
    int stock;

public:
    Product(int id, string name, double price, int stock)
        : id(id), name(std::move(name)), price(price), stock(stock) {}

    // Getters
    int getId() const { return id; }
    const string& getName() const { return name; }
    double getPrice() const { return price; }
    int getStock() const { return stock; }

    // Business methods
    bool canSell(int qty) const {
        return qty > 0 && qty <= stock;
    }

    bool sell(int qty) {
        if (!canSell(qty)) return false;
        stock -= qty;
        return true;
    }

    void restock(int qty) {
        if (qty > 0) stock += qty;
    }

    void print() const {
        cout << "Product[" << id << "] " << name
             << " | RM " << price
             << " | Stock: " << stock << "\n";
    }
};

int main() {
    Product p(301, "Mineral Water", 2.50, 10);

    p.print();

    cout << "Sell 3? " << (p.sell(3) ? "OK" : "FAIL") << "\n";
    p.print();

    cout << "Sell 99? " << (p.sell(99) ? "OK" : "FAIL") << "\n";
    p.print();

    p.restock(20);
    cout << "After restock:\n";
    p.print();

    return 0;
}
