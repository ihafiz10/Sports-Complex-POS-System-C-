#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <fstream>
#include <limits>
#include <ctime>
#include <map>
#include <algorithm>
#include <cctype>

using namespace std;

// ================= DATA STRUCTURES =================
struct Member {
    string name;
    string phone;
    string type;       // "6 Months" or "12 Months"
    string joinDate;   // "DD/MM/YYYY"
    string expiryDate; // Calculated
};

struct Product {
    int id;
    string name;
    double price;
    int stock;
};

struct BookingRecord {
    int bookingID;
    string customerName;
    string customerPhone;
    double totalAmount;
    string dateStr; // "DD/MM/YYYY HH:MM"
    vector<string> items;
};

struct DailyReport {
    string date; // "DD/MM/YYYY"
    int totalTransactions = 0;
    double totalSales = 0.0;
    string bestBooking = "-";
    string bestRental = "-";
    string bestProduct = "-";
};

struct MonthlyReport {
    string month; // "YYYY-MM"
    int totalTransactions = 0;
    double totalSales = 0.0;
    string bestBooking = "-";
    string bestRental = "-";
    string bestProduct = "-";
    double highestIncome = 0.0;
};

// ================= GLOBAL VARIABLES =================
vector<Member> members;
vector<Product> inventory;
vector<string> feedbackList;
vector<BookingRecord> allBookings;
vector<DailyReport> dailyReports;
vector<MonthlyReport> monthlyReports;

int nextBookingID = 1001;

// Cart
vector<string> currentCartItems;
double currentCartTotal = 0.0;

// Files
const char* FILE_MEMBERS = "members.txt";
const char* FILE_INVENTORY = "inventory.txt";
const char* FILE_BOOKINGS = "bookings.txt";
const char* FILE_FEEDBACKS = "feedbacks.txt";

// ================= UTIL HELPERS =================
static inline void clearBadInput() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

static inline string trimCopy(string s) {
    while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
    return s;
}

// Accept HHMM where HH=0..23 and MM=0..59
bool isValidHHMM(int hhmm) {
    int hh = hhmm / 100;
    int mm = hhmm % 100;
    if (hh < 0 || hh > 23) return false;
    if (mm < 0 || mm > 59) return false;
    return true;
}

int toMinutes(int hhmm) {
    int hh = hhmm / 100;
    int mm = hhmm % 100;
    return hh * 60 + mm;
}

bool parseCourtBookingItem(const string& item, string& facility, string& date, int& startHHMM, int& endHHMM) {
    if (item.rfind("Booking:", 0) != 0) return false;

    size_t lb = item.find('[');
    size_t rb = item.find(']');
    if (lb == string::npos || rb == string::npos || rb <= lb) return false;

    facility = trimCopy(item.substr(8, lb - 8)); // after "Booking:"
    if (facility.find("Court") == string::npos) return false; // only court booking

    string inside = trimCopy(item.substr(lb + 1, rb - lb - 1)); // "16/12/2025 1330-1430"
    if (inside.size() < 10) return false;

    date = inside.substr(0, 10); // DD/MM/YYYY

    size_t sp = inside.find(' ');
    if (sp == string::npos) return false;

    string times = trimCopy(inside.substr(sp + 1)); // "1330-1430"

    size_t dash = times.find('-');
    if (dash == string::npos) dash = times.find("–"); // support en-dash too
    if (dash == string::npos) return false;

    try {
        startHHMM = stoi(trimCopy(times.substr(0, dash)));
        endHHMM = stoi(trimCopy(times.substr(dash + 1)));
    }
    catch (...) {
        return false;
    }

    if (!isValidHHMM(startHHMM) || !isValidHHMM(endHHMM)) return false;
    return true;
}

bool hasCourtClash(const string& facility, const string& date, int startHHMM, int endHHMM) {
    int startMin = toMinutes(startHHMM);
    int endMin = toMinutes(endHHMM);

    for (const auto& b : allBookings) {
        for (const auto& it : b.items) {
            string f, d;
            int sHHMM, eHHMM;
            if (!parseCourtBookingItem(it, f, d, sHHMM, eHHMM)) continue;

            if (f == facility && d == date) {
                int sMin = toMinutes(sHHMM);
                int eMin = toMinutes(eHHMM);

                // overlap if NOT (new ends before old starts OR new starts after old ends)
                if (!(endMin <= sMin || startMin >= eMin)) return true;
            }
        }
    }
    return false;
}


static inline bool startsWith(const string& s, const string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

void pause() {
    cout << "\n----------------------------------------";
    cout << "\nPress Enter to return to Main Menu...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cin.get();
}

// ================= DATE / TIME HELPERS =================
bool isLeapYear(int y) {
    return ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
}

bool isValidDate(int day, int month, int year) {
    if (year < 1900 || year > 2100) return false;
    if (month < 1 || month > 12) return false;
    if (day < 1) return false;

    int daysInMonth[] = { 0,31,28,31,30,31,30,31,31,30,31,30,31 };
    if (month == 2 && isLeapYear(year)) daysInMonth[2] = 29;
    return day <= daysInMonth[month];
}

bool parseDateDDMMYYYY(const string& s, int& d, int& m, int& y) {
    if (s.size() != 10 || s[2] != '/' || s[5] != '/') return false;
    try {
        d = stoi(s.substr(0, 2));
        m = stoi(s.substr(3, 2));
        y = stoi(s.substr(6, 4));
    }
    catch (...) {
        return false;
    }
    return isValidDate(d, m, y);
}


string getCurrentTimestamp(bool includeTime = false) {
    time_t t = time(nullptr);
    tm now{};
#ifdef _WIN32
    localtime_s(&now, &t);
#else
    localtime_r(&t, &now);
#endif
    char buffer[32];
    if (includeTime)
        strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M", &now);
    else
        strftime(buffer, sizeof(buffer), "%d/%m/%Y", &now);

    return string(buffer);
}

string calculateExpiry(const string& joinDate, int durationMonths) {
    int d, m, y;
    if (!parseDateDDMMYYYY(joinDate, d, m, y)) return "Invalid Date";

    m += durationMonths;
    while (m > 12) { m -= 12; y++; }

    string dd = (d < 10 ? "0" : "") + to_string(d);
    string mm = (m < 10 ? "0" : "") + to_string(m);
    return dd + "/" + mm + "/" + to_string(y);
}

static inline string formatDDMMYYYY(int d, int m, int y) {
    string dd = (d < 10 ? "0" : "") + to_string(d);
    string mm = (m < 10 ? "0" : "") + to_string(m);
    return dd + "/" + mm + "/" + to_string(y);
}

// ================= CART HELPERS =================
void addToCart(const string& itemName, double price, bool silent = false) {
    currentCartItems.push_back(itemName);
    currentCartTotal += price;
    if (!silent) {
        cout << ">> Added " << itemName << " (RM " << fixed << setprecision(2) << price << ") to bill.\n";
    }
}

void clearCart() {
    currentCartItems.clear();
    currentCartTotal = 0.0;
    cout << "Cart cleared.\n";
}

// ================= PROMO =================
double getPromoDiscount(double subtotal) {
    char hasCode;
    do {
        cout << "\nDo you have a promo code? (Y/N): ";
        cin >> hasCode;
        if (hasCode != 'Y' && hasCode != 'y' && hasCode != 'N' && hasCode != 'n')
            cout << "Invalid input! Please enter Y or N.\n";
    } while (hasCode != 'Y' && hasCode != 'y' && hasCode != 'N' && hasCode != 'n');

    if (hasCode == 'N' || hasCode == 'n') return 0.0;

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    while (true) {
        string code;
        cout << "Enter promo code (or type EXIT to skip): ";
        getline(cin, code);
        code = trimCopy(code);

        if (code == "EXIT") return 0.0;

        // Support both versions + extra
        if (code == "PROMO10") {
            cout << ">> Promo applied: 10% OFF\n";
            return subtotal * 0.10;
        }
        if (code == "DISC5" || code == "SPORT2025") {
            cout << ">> Promo applied: RM 5 OFF\n";
            return 5.0;
        }

        cout << "ERROR! Invalid promo code.\n";
    }
}

// ================= FILE HANDLING =================
void loadDefaultsInventory() {
    inventory.clear();
    // Sports Products
    inventory.push_back({ 201, "Badminton Racket", 100.00, 10 });
    inventory.push_back({ 202, "Badminton Strings", 20.00, 20 });
    inventory.push_back({ 203, "Shuttlecocks (12)", 70.00, 30 });
    inventory.push_back({ 204, "Paddle", 100.00, 10 });
    inventory.push_back({ 205, "Pickle Ball (6)", 50.00, 20 });
    inventory.push_back({ 206, "Basketball", 100.00, 10 });
    inventory.push_back({ 207, "Goggles", 20.00, 15 });
    inventory.push_back({ 208, "Swim Cap", 5.00, 20 });
    // Drinks & Snacks
    inventory.push_back({ 301, "Mineral Water", 2.50, 99 });
    inventory.push_back({ 302, "100 Plus / Coca-Cola", 4.00, 50 });
    inventory.push_back({ 303, "Gatorade", 4.00, 40 });
    inventory.push_back({ 304, "Jasmine / Lemon Tea", 3.50, 40 });
    inventory.push_back({ 305, "Redbull", 5.00, 30 });
    inventory.push_back({ 306, "Milo", 3.50, 40 });
    inventory.push_back({ 307, "Protein Shake", 10.00, 20 });
    inventory.push_back({ 308, "Biscuit", 5.00, 30 });
    inventory.push_back({ 309, "Chocolate / Energy Bar", 3.50, 50 });
}

void loadData() {
    // Members
    {
        ifstream memberFile(FILE_MEMBERS);
        if (memberFile) {
            Member m;
            while (getline(memberFile, m.name)) {
                getline(memberFile, m.phone);
                getline(memberFile, m.type);
                getline(memberFile, m.joinDate);
                getline(memberFile, m.expiryDate);
                if (!m.name.empty()) members.push_back(m);
            }
        }
    }

    // Inventory
    {
        ifstream invFile(FILE_INVENTORY);
        if (invFile) {
            Product p;
            while (invFile >> p.id) {
                invFile.ignore();
                getline(invFile, p.name);
                invFile >> p.price;
                invFile >> p.stock;
                inventory.push_back(p);
            }
        }
        else {
            loadDefaultsInventory();
        }
    }

    // Bookings
    {
        ifstream bookFile(FILE_BOOKINGS);
        if (bookFile) {
            BookingRecord b;
            int itemCount;
            while (bookFile >> b.bookingID) {
                bookFile.ignore();
                getline(bookFile, b.customerName);
                getline(bookFile, b.customerPhone);
                getline(bookFile, b.dateStr);
                bookFile >> b.totalAmount;
                bookFile >> itemCount;
                bookFile.ignore();

                b.items.clear();
                for (int i = 0; i < itemCount; i++) {
                    string itemLine;
                    getline(bookFile, itemLine);
                    b.items.push_back(itemLine);
                }
                allBookings.push_back(b);

                if (b.bookingID >= nextBookingID)
                    nextBookingID = b.bookingID + 1;
            }
        }
    }

    // Feedback
    {
        ifstream feedFile(FILE_FEEDBACKS);
        if (feedFile) {
            string line;
            while (getline(feedFile, line)) {
                line = trimCopy(line);
                if (!line.empty()) feedbackList.push_back(line);
            }
        }
    }
}

void saveData() {
    {
        ofstream memberFile(FILE_MEMBERS);
        for (const auto& m : members) {
            memberFile << m.name << "\n" << m.phone << "\n" << m.type << "\n"
                << m.joinDate << "\n" << m.expiryDate << "\n";
        }
    }
    {
        ofstream invFile(FILE_INVENTORY);
        for (const auto& item : inventory) {
            invFile << item.id << "\n" << item.name << "\n" << item.price << "\n" << item.stock << "\n";
        }
    }
    {
        ofstream bookFile(FILE_BOOKINGS);
        for (const auto& b : allBookings) {
            bookFile << b.bookingID << "\n";
            bookFile << b.customerName << "\n";
            bookFile << b.customerPhone << "\n";
            bookFile << b.dateStr << "\n";
            bookFile << b.totalAmount << "\n";
            bookFile << b.items.size() << "\n";
            for (const auto& it : b.items) bookFile << it << "\n";
        }
    }
    {
        ofstream feedFile(FILE_FEEDBACKS);
        for (const auto& f : feedbackList) feedFile << f << "\n";
    }

    cout << "[System] All data saved successfully.\n";
}

// ================= REPORT HELPERS =================
string getBestItem(const map<string, int>& counts) {
    string best = "-";
    int mx = 0;
    for (const auto& kv : counts) {
        if (kv.second > mx) {
            mx = kv.second;
            best = kv.first + " (" + to_string(kv.second) + " sold)";
        }
    }
    return best;
}

// Parse cart item into category + name + qty
static inline void categorizeItem(const string& item, string& cat, string& name, int& qty) {
    cat = "Other";
    name = item;
    qty = 1;

    // Skip locker from "best" stats, but keep in bill/sales by default
    if (startsWith(item, "Locker:") || item.find("Locker") != string::npos) {
        cat = "Locker";
        name = trimCopy(item);
        qty = 1;
        return;
    }

    if (startsWith(item, "Booking:")) {
        cat = "Booking";
        // Booking: Name [date ...]
        size_t start = string("Booking:").size();
        while (start < item.size() && item[start] == ' ') start++;
        size_t end = item.find("[");
        if (end != string::npos && end > start) name = trimCopy(item.substr(start, end - start));
        else name = trimCopy(item.substr(start));
        qty = 1;
        return;
    }

    if (startsWith(item, "Rent:")) {
        cat = "Rent";
        name = trimCopy(item.substr(string("Rent:").size()));
        qty = 1;
        return;
    }

    if (startsWith(item, "Buy:")) {
        cat = "Product";
        // Buy: X x N
        string rest = trimCopy(item.substr(string("Buy:").size()));
        size_t pos = rest.find(" x ");
        if (pos != string::npos) {
            name = trimCopy(rest.substr(0, pos));
            try { qty = stoi(trimCopy(rest.substr(pos + 3))); }
            catch (...) { qty = 1; }
        }
        else {
            name = rest;
            qty = 1;
        }
        return;
    }
}

void generateDailyReports() {
    dailyReports.clear();

    map<string, map<string, map<string, int>>> tracker; // date->cat->name->count
    map<string, double> sales;
    map<string, int> txns;

    for (const auto& b : allBookings) {
        if (b.dateStr.size() < 10) continue;
        string date = b.dateStr.substr(0, 10); // DD/MM/YYYY

        sales[date] += b.totalAmount;
        txns[date]++;

        for (const string& item : b.items) {
            string cat, name;
            int qty = 1;
            categorizeItem(item, cat, name, qty);

            // For "best" stats, ignore lockers
            if (cat == "Locker") continue;

            tracker[date][cat][name] += max(1, qty);
        }
    }

    for (const auto& kv : tracker) {
        DailyReport r;
        r.date = kv.first;
        r.totalSales = sales[r.date];
        r.totalTransactions = txns[r.date];
        r.bestBooking = getBestItem(tracker[r.date]["Booking"]);
        r.bestRental = getBestItem(tracker[r.date]["Rent"]);
        r.bestProduct = getBestItem(tracker[r.date]["Product"]);
        dailyReports.push_back(r);
    }

    // Sort by date string (DD/MM/YYYY) lexicographic isn't perfect across months;
    // keep insertion order ok. If you want exact sorting, we can convert to y-m-d.
}

void generateMonthlyReports() {
    monthlyReports.clear();

    map<string, map<string, map<string, int>>> tracker; // month->cat->name->count
    map<string, double> sales;
    map<string, int> txns;
    map<string, double> highestBill;

    for (const auto& b : allBookings) {
        if (b.dateStr.size() < 10) continue;

        // "DD/MM/YYYY ..."
        string monthKey = b.dateStr.substr(6, 4) + "-" + b.dateStr.substr(3, 2); // YYYY-MM

        sales[monthKey] += b.totalAmount;
        txns[monthKey]++;
        highestBill[monthKey] = max(highestBill[monthKey], b.totalAmount);

        for (const string& item : b.items) {
            string cat, name;
            int qty = 1;
            categorizeItem(item, cat, name, qty);

            if (cat == "Locker") continue; // ignore in best stats
            tracker[monthKey][cat][name] += max(1, qty);
        }
    }

    for (const auto& kv : tracker) {
        MonthlyReport r;
        r.month = kv.first;
        r.totalSales = sales[r.month];
        r.totalTransactions = txns[r.month];
        r.highestIncome = highestBill[r.month];
        r.bestBooking = getBestItem(tracker[r.month]["Booking"]);
        r.bestRental = getBestItem(tracker[r.month]["Rent"]);
        r.bestProduct = getBestItem(tracker[r.month]["Product"]);
        monthlyReports.push_back(r);
    }
}

void checkDailyReport() {
    cout << "\n--- Daily Reports ---\n";
    if (dailyReports.empty()) { cout << "No data available.\n"; return; }

    for (const auto& r : dailyReports) {
        cout << "========================================\n";
        cout << "DATE: " << r.date << "\n";
        cout << "Transactions: " << r.totalTransactions << "\n";
        cout << "Total Sales : RM " << fixed << setprecision(2) << r.totalSales << "\n";
        cout << "----------------------------------------\n";
        cout << "Top Booking : " << r.bestBooking << "\n";
        cout << "Top Rental  : " << r.bestRental << "\n";
        cout << "Top Product : " << r.bestProduct << "\n";
        cout << "========================================\n\n";
    }
}

void checkMonthlyReport() {
    cout << "\n--- Monthly Reports ---\n";
    if (monthlyReports.empty()) { cout << "No data available.\n"; return; }

    for (const auto& r : monthlyReports) {
        cout << "========================================\n";
        cout << "MONTH: " << r.month << "\n";
        cout << "Transactions: " << r.totalTransactions << "\n";
        cout << "Total Sales : RM " << fixed << setprecision(2) << r.totalSales << "\n";
        cout << "Highest Bill: RM " << fixed << setprecision(2) << r.highestIncome << "\n";
        cout << "----------------------------------------\n";
        cout << "Top Booking : " << r.bestBooking << "\n";
        cout << "Top Rental  : " << r.bestRental << "\n";
        cout << "Top Product : " << r.bestProduct << "\n";
        cout << "========================================\n\n";
    }
}

// ================= UI / MODULES =================
void displayIntro() {
    cout << "\n\n";
    cout << "\t=======================================================\n";
    cout << "\t||                                                   ||\n";
    cout << "\t||           WELCOME TO RSW SPORTS COMPLEX           ||\n";
    cout << "\t||                   POS SYSTEM                      ||\n";
    cout << "\t||                                                   ||\n";
    cout << "\t=======================================================\n";
    cout << "\t||                                                   ||\n";
    cout << "\t||   > BADMINTON   > PICKLEBALL   > BASKETBALL       ||\n";
    cout << "\t||         > SWIMMING POOL   > GYM & FITNESS         ||\n";
    cout << "\t||                                                   ||\n";
    cout << "\t=======================================================\n";
    cout << "\n\t[System] Initializing Database...\n";
    cout << "\t[System] Ready.\n\n";
    cout << "\tPress Enter to Start...";
    cin.get();
}

static bool isDigitsOnly(const string& s) {
    if (s.empty()) return false;
    for (char c : s) if (!isdigit((unsigned char)c)) return false;
    return true;
}

void applyMembership() {
    cout << "\n-- Apply Membership --\n";
    cout << "(At NAME: type 0 to return Main Menu)\n";
    cout << "(At other steps: any invalid input will restart from NAME)\n";

    // Clear leftover newline from main menu choice
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    while (true) {
        // ============ NAME ============
        string name;
        while (true) {
            cout << "\nEnter Full Name: ";
            getline(cin, name);
            name = trimCopy(name);

            if (name == "0") return; // ONLY here goes main menu

            if (name.empty()) { cout << "Invalid name.\n"; continue; }

            bool ok = true;
            for (char c : name) {
                if (!isalpha((unsigned char)c) && !isspace((unsigned char)c) && !isdigit((unsigned char)c)) {
                    ok = false; break;
                }
            }
            if (!ok) { cout << "Invalid name.\n"; continue; }
            break;
        }

        // ============ PHONE ============
        string phone;
        cout << "Enter Phone Number (ID): ";
        getline(cin, phone);
        phone = trimCopy(phone);

        // Any invalid phone -> go back to NAME
        if (!isDigitsOnly(phone)) {
            cout << "Invalid phone. Restarting from NAME...\n";
            continue;
        }

        // ============ JOIN DATE (AUTO) ============
        string joinDate = getCurrentTimestamp(false);
        cout << "Start Date: " << joinDate << " (Auto)\n";

        // ============ PLAN ============
        string choice, type;
        double price = 0.0;
        int durationMonths = 0;

        cout << "\n-- Membership Plan --\n";
        cout << "1. 6 Months  (RM  60)\n";
        cout << "2. 12 Months (RM 100)\n";
        cout << "Select Type: ";
        getline(cin, choice);
        choice = trimCopy(choice);

        // Any invalid plan -> go back to NAME
        if (choice == "1") { price = 60.0; type = "6 Months"; durationMonths = 6; }
        else if (choice == "2") { price = 100.0; type = "12 Months"; durationMonths = 12; }
        else {
            cout << "Invalid option. Restarting from NAME...\n";
            continue;
        }

        string expiry = calculateExpiry(joinDate, durationMonths);

        cout << "\n----------------------------\n";
        cout << left << setw(15) << "Fee" << ": RM " << fixed << setprecision(2) << price << "\n";
        cout << left << setw(15) << "Duration" << ": " << durationMonths << " Months\n";
        cout << left << setw(15) << "Valid Until" << ": " << expiry << "\n";
        cout << "----------------------------\n";

        // ============ PAYMENT ============
        cout << "Select Payment: 1. Card 2. E-Wallet\nChoice: ";
        getline(cin, choice);
        choice = trimCopy(choice);

        // Any invalid payment -> go back to NAME
        if (!(choice == "1" || choice == "2")) {
            cout << "Invalid payment choice. Restarting from NAME...\n";
            continue;
        }

        // ============ SAVE ============
        members.push_back({ name, phone, type, joinDate, expiry });

        saveData();

        cout << "\n=========================================\n";
        cout << "   MEMBERSHIP REGISTERED SUCCESSFULLY!   \n";
        cout << "   Welcome, " << name << "!\n";
        cout << "   Valid Until: " << expiry << "\n";
        cout << "=========================================\n";
        return;
    }
}


void facilityInfo() {
    cout << "\n-- Facility Prices (Per Hour) --\n";
    cout << "Sport:\t\tWeekday\t\tWeekend\n";
    cout << "Badminton\tRM   15\t\tRM   20\n";
    cout << "Pickleball\tRM   15\t\tRM   20\n";
    cout << "Basketball\tRM   40\t\tRM   50\n";

    cout << "\n-- Per Entry Prices --\n";
    cout << "Sport:\t\t\t\tWeekday\t\tWeekend\n";
    cout << "Swimming Pool\t\t\tRM    5\t\tRM    8\n";
    cout << "Gym Room\t\t\tRM   10\t\tRM   10\n";
    cout << "Fitness Studio (Max 10 peope)\tRM   30\t\tRM   30\n";
}

bool isTodayOrTomorrow(int d, int m, int y) {
    time_t now = time(nullptr);
    tm t{};
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0;
    time_t today = mktime(&t);

    tm target{};
    target.tm_year = y - 1900;
    target.tm_mon = m - 1;
    target.tm_mday = d;
    target.tm_hour = 0; target.tm_min = 0; target.tm_sec = 0;
    time_t tt = mktime(&target);

    double diffDays = difftime(tt, today) / 86400.0;
    return (diffDays >= 0.0 && diffDays <= 1.0);
}

string getDateOffsetStr(int offsetDays) {
    time_t now = time(nullptr);
    tm t{};
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    t = *localtime(&now);
#endif
    t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0;

    time_t base = mktime(&t);
    base += (time_t)offsetDays * 86400;

#ifdef _WIN32
    localtime_s(&t, &base);
#else
    t = *localtime(&base);
#endif

    char buf[16];
    strftime(buf, sizeof(buf), "%d/%m/%Y", &t);
    return string(buf);
}

bool isWeekendDate(const string& ddmmyyyy) {
    int d, m, y;
    if (!parseDateDDMMYYYY(ddmmyyyy, d, m, y)) return false;

    tm t{};
    t.tm_year = y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_hour = 12; // safer for DST

    time_t tt = mktime(&t);
    if (tt == -1) return false;

    tm out{};
#ifdef _WIN32
    localtime_s(&out, &tt);
#else
    localtime_r(&tt, &out);
#endif

    return (out.tm_wday == 0 || out.tm_wday == 6); // Sunday or Saturday
}



void bookFacility() {
    cout << "\n-- Book Facility --\n";
    cout << "(At Date Selection: type 0 to return Main Menu)\n";
    cout << "(Any invalid input will restart from Date Selection)\n";
    cout << "(Time clash ONLY: re-enter time, no restart)\n";

    while (true) {
        // =============== DATE PICK (AUTO) ===============
        string todayStr = getDateOffsetStr(0);
        string tomorrowStr = getDateOffsetStr(1);
        string dateStr;

        while (true) {
            cout << "\n-- Date Selection --\n";
            cout << "1. Today    (" << todayStr << ")\n";
            cout << "2. Tomorrow (" << tomorrowStr << ")\n";
            cout << "0. Return to Main Menu\n";
            cout << "Select: ";

            int pick;
            if (!(cin >> pick)) { clearBadInput(); cout << "Invalid option.\n"; continue; }
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            if (pick == 0) return;
            if (pick == 1) { dateStr = todayStr; break; }
            if (pick == 2) { dateStr = tomorrowStr; break; }

            cout << "Invalid option.\n";
        }

        bool weekend = isWeekendDate(dateStr);
        cout << "Date accepted: " << dateStr << " (" << (weekend ? "Weekend" : "Weekday") << ")\n";

        // =============== TYPE ===============
        cout << "\nOperation Hours: 10:00 AM - 10:00 PM\n";
        cout << "1. Hourly Court Booking\n2. Per Entry Pass\nSelect: ";

        int typeChoice;
        if (!(cin >> typeChoice)) { clearBadInput(); cout << "Invalid option. Restarting...\n"; continue; }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        // =============== PER ENTRY ===============
        if (typeChoice == 2) {
            int entryChoice;
            cout << "\n1. Swimming Pool\n2. Gym Room\n3. Fitness Studio\nSelect: ";
            if (!(cin >> entryChoice)) { clearBadInput(); cout << "Invalid option. Restarting...\n"; continue; }
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            double price = 0.0;
            string name;

            if (entryChoice == 1) {
                price = weekend ? 8.0 : 5.0;
                name = "Swimming Pool Entry";
                cout << "Detected: " << (weekend ? "Weekend (RM 8)\n" : "Weekday (RM 5)\n");
            }
            else if (entryChoice == 2) {
                price = 10.0; name = "Gym Room Entry";
            }
            else if (entryChoice == 3) {
                price = 30.0; name = "Fitness Studio Entry";
            }
            else {
                cout << "Invalid option. Restarting...\n";
                continue;
            }

            addToCart("Booking: " + name + " [" + dateStr + "]", price);
            return;
        }

        if (typeChoice != 1) {
            cout << "Invalid option. Restarting...\n";
            continue;
        }

        // =============== COURT SELECTION ===============
        int sport;
        cout << "\nSport:\n1. Badminton\n2. Pickleball\n3. Basketball\nSelect: ";
        if (!(cin >> sport)) { clearBadInput(); cout << "Invalid option. Restarting...\n"; continue; }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        string courtName;
        double wk = 0, wknd = 0;

        if (sport == 1) { courtName = "Badminton Court"; wk = 15; wknd = 20; }
        else if (sport == 2) { courtName = "Pickleball Court"; wk = 15; wknd = 20; }
        else if (sport == 3) { courtName = "Basketball Court"; wk = 40; wknd = 50; }
        else { cout << "Invalid option. Restarting...\n"; continue; }

        double hourlyRate = weekend ? wknd : wk;
        cout << "Auto Rate (" << (weekend ? "Weekend" : "Weekday") << "): RM " << hourlyRate << " / hour\n";

        // =============== TIME SELECTION ===============
        while (true) {
            int startTime = 0, endTime = 0;

            string s;
            cout << "\nStart Time (HHMM, e.g. 1330): ";
            getline(cin, s);
            try { startTime = stoi(trimCopy(s)); }
            catch (...) { cout << "Invalid option. Restarting...\n"; break; }

            cout << "End Time   (HHMM, e.g. 1430): ";
            getline(cin, s);
            try { endTime = stoi(trimCopy(s)); }
            catch (...) { cout << "Invalid option. Restarting...\n"; break; }

            if (!isValidHHMM(startTime) || !isValidHHMM(endTime)) {
                cout << "Invalid time format. Restarting...\n";
                break; // restart date selection
            }

            int startMin = toMinutes(startTime);
            int endMin = toMinutes(endTime);

            // Operating hours: 10:00 to 22:00
            if (startMin < 10 * 60 || endMin > 22 * 60) {
                cout << "Invalid time (outside operation hours). Restarting...\n";
                break;
            }

            if (endMin <= startMin) {
                cout << "Invalid time (end must be after start). Restarting...\n";
                break;
            }

            if (endMin - startMin < 60) {
                cout << "Minimum booking duration is 60 minutes. Restarting...\n";
                break;
            }

            // If booking is TODAY and start time already passed -> restart
            if (dateStr == todayStr) {
                time_t now = time(nullptr);
                tm cur{};
#ifdef _WIN32
                localtime_s(&cur, &now);
#else
                cur = *localtime(&now);
#endif
                int currentMin = cur.tm_hour * 60 + cur.tm_min;

                if (startMin <= currentMin) {
                    cout << "Error: Time has already passed for today. Restarting...\n";
                    break;
                }
            }

            // Clash check (ONLY re-enter time, do NOT restart)
            if (hasCourtClash(courtName, dateStr, startTime, endTime)) {
                cout << "ERROR: This slot is already booked for " << courtName
                    << " on " << dateStr << ".\n";
                cout << "Please choose another time.\n";
                continue; // re-enter time only
            }

            double hours = (endMin - startMin) / 60.0;  // supports 90 mins = 1.5
            double total = hourlyRate * hours;

            addToCart("Booking: " + courtName + " [" + dateStr + " " +
                to_string(startTime) + "-" + to_string(endTime) + "]", total);
            return;
        }

        // If we broke out of time loop (invalid time etc.), restart at Date Selection
        continue;
    }
}






void rentEquipment() {
    while (true) {
        int choice, qty;

        cout << "\n-- Rent Equipment --\n";
        cout << "Equipment\t\tPrice\t\tDeposit\n";
        cout << "1. Badminton Racket\tRM 10\t\tRM 30\n";
        cout << "2. Paddle\t\tRM  8\t\tRM 30\n";
        cout << "3. Basketball\t\tRM 10\t\tRM 30\n";
        cout << "4. Resistance Band\tRM  5\t\t-\n";
        cout << "5. Yoga Mat\t\tRM  5\t\t-\n";
        cout << "0. Return\n";
        cout << "Select Item: ";

        if (!(cin >> choice)) { clearBadInput(); continue; }
        if (choice == 0) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return;
        }

        cout << "Enter Quantity: ";
        if (!(cin >> qty)) { clearBadInput(); continue; }
        if (qty <= 0) { cout << "Quantity must be at least 1.\n"; continue; }

        string itemName;
        double price = 0.0;
        int depositPerUnit = 0; // 0 = no deposit

        switch (choice) {
        case 1: itemName = "Rent: Badminton Racket"; price = 10.0; depositPerUnit = 30; break;
        case 2: itemName = "Rent: Paddle";          price = 8.0; depositPerUnit = 30; break;
        case 3: itemName = "Rent: Basketball";      price = 10.0; depositPerUnit = 30; break;
        case 4: itemName = "Rent: Resistance Band"; price = 5.0; depositPerUnit = 0;  break;
        case 5: itemName = "Rent: Yoga Mat";        price = 5.0; depositPerUnit = 0;  break;
        default:
            cout << "Invalid choice.\n";
            continue;
        }

        for (int i = 0; i < qty; i++) {
            addToCart(itemName, price, true); // silent adds
        }

        cout << ">> Added " << qty << " item(s): " << itemName << "\n";

        if (depositPerUnit > 0) {
            cout << ">> Deposit at counter: RM " << (depositPerUnit * qty)
                << " (RM " << depositPerUnit << " x " << qty << ")\n";
        }

        cout << ">> Current bill: RM " << fixed << setprecision(2) << currentCartTotal << "\n";
    }
}


void buyMerchandiseSnacks() {
    while (true) {
        cout << "\n-- Buy Equipment & Snacks --\n\n";
        cout << left << setw(5) << "ID"
            << setw(30) << "Product"
            << setw(12) << "Price(RM)"
            << right << setw(10) << "Stock" << "\n";
        cout << "--------------------------------------------------------\n";

        for (const auto& item : inventory) {
            cout << left << setw(5) << item.id
                << setw(30) << item.name
                << setw(12) << fixed << setprecision(2) << item.price
                << right << setw(10) << item.stock << "\n";
        }

        int id;
        cout << "\nEnter ID (0 to return): ";
        if (!(cin >> id)) { clearBadInput(); continue; }

        if (id == 0) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return; // back to main menu
        }

        // Find product
        auto it = find_if(inventory.begin(), inventory.end(),
            [&](const Product& p) { return p.id == id; });

        if (it == inventory.end()) {
            cout << "Item ID not found.\n";
            continue; // loop again
        }

        if (it->stock <= 0) {
            cout << "Sorry. " << it->name << " is out of stock.\n";
            continue; // loop again
        }

        int qty;
        cout << "Enter Quantity (1-" << it->stock << "): ";
        if (!(cin >> qty)) { clearBadInput(); continue; }

        if (qty <= 0) {
            cout << "Quantity must be at least 1.\n";
            continue;
        }
        if (qty > it->stock) {
            cout << "Not enough stock! We only have " << it->stock << " units.\n";
            continue;
        }

        it->stock -= qty;

        addToCart("Buy: " + it->name + " x " + to_string(qty), it->price * qty);
        cout << ">> Added to cart successfully!\n";
        cout << ">> Current bill: RM " << fixed << setprecision(2) << currentCartTotal << "\n";
    }
}



void lockerRental() {
    int choice;
    cout << "\n-- Locker Rental --\n";
    cout << "1. Small  (RM 5)\n";
    cout << "2. Medium (RM 8)\n";
    cout << "3. Large  (RM10)\n";
    cout << "Select (0 cancel): ";

    if (!(cin >> choice)) { clearBadInput(); return; }
    if (choice == 0) return;

    double price = 0.0;
    string label;

    if (choice == 1) { label = "Locker: Small"; price = 5.0; }
    else if (choice == 2) { label = "Locker: Medium"; price = 8.0; }
    else if (choice == 3) { label = "Locker: Large"; price = 10.0; }
    else { cout << "Invalid.\n"; return; }

    cout << label << " is RM " << fixed << setprecision(2) << price << ". Proceed? (y/n): ";
    char yn;
    cin >> yn;
    if (yn == 'y' || yn == 'Y') {
        addToCart(label, price);
        cout << "Locker added. Please take the key at counter.\n";
    }
    else {
        cout << "Cancelled.\n";
    }
}

void checkoutPayment() {
    cout << "\n-- Checkout & Payment --\n";
    if (currentCartTotal <= 0.0) {
        cout << "Cart is empty.\n";
        return;
    }

    // Identify customer
    string finalName, finalPhone, phoneInput;
    bool isMember = false;

    cout << "Are you a member? Enter phone number (or 'N' for No): ";
    cin >> phoneInput;

    if (phoneInput != "N" && phoneInput != "n") {
        for (const auto& m : members) {
            if (m.phone == phoneInput) {
                isMember = true;
                finalName = m.name;
                finalPhone = m.phone;
                cout << ">> Member Identified: " << finalName << "\n";
                break;
            }
        }
        if (!isMember) {
            cout << "Member not found. Proceeding as Non-Member.\n";
        }
    }

    if (!isMember) {
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        while (true) {
            cout << "Enter Full Name: ";
            getline(cin, finalName);
            finalName = trimCopy(finalName);
            if (!finalName.empty()) break;
            cout << "Invalid name.\n";
        }
        while (true) {
            cout << "Enter Phone Number (ID): ";
            getline(cin, finalPhone);
            finalPhone = trimCopy(finalPhone);
            if (isDigitsOnly(finalPhone)) break;
            cout << "Invalid phone.\n";
        }
    }

    // Bill summary (group same lines)
    cout << "\n--- BILL SUMMARY ---\n";
    map<string, int> billCounts;
    for (const auto& s : currentCartItems) billCounts[s]++;

    for (const auto& kv : billCounts) {
        cout << "- " << kv.first;
        if (kv.second > 1) cout << " x " << kv.second;
        cout << "\n";
    }

    double subtotal = currentCartTotal;
    cout << "------------------------------\n";
    cout << left << setw(18) << "Subtotal (RM):" << right << setw(10) << fixed << setprecision(2) << subtotal << "\n";

    // Promo
    double promoDiscount = getPromoDiscount(subtotal);
    double afterPromo = subtotal - promoDiscount;
    if (afterPromo < 0) afterPromo = 0;

    if (promoDiscount > 0) {
        cout << left << setw(18) << "Promo (-RM):" << right << setw(10) << fixed << setprecision(2) << promoDiscount << "\n";
    }

    // Member discount
    double memberDiscount = 0.0;
    double afterMember = afterPromo;
    if (isMember) {
        memberDiscount = afterPromo * 0.10;
        afterMember -= memberDiscount;
        cout << left << setw(18) << "Member (-RM):" << right << setw(10) << fixed << setprecision(2) << memberDiscount << "\n";
    }

    // SST
    double tax = afterMember * 0.06;
    double grandTotal = afterMember + tax;

    cout << left << setw(18) << "SST 6% (RM):" << right << setw(10) << fixed << setprecision(2) << tax << "\n";
    cout << "------------------------------\n";
    cout << left << setw(18) << "GRAND TOTAL:" << right << setw(10) << fixed << setprecision(2) << grandTotal << "\n";
    cout << "------------------------------\n";

    // Payment method
    int payMethod;
    cout << "Select Payment: 1. Card 2. E-Wallet\nChoice: ";
    cin >> payMethod;

    // Create booking record
    BookingRecord b;
    b.bookingID = nextBookingID++;
    b.customerName = finalName;
    b.customerPhone = finalPhone;
    b.totalAmount = grandTotal;
    b.items = currentCartItems;
    b.dateStr = getCurrentTimestamp(true);

    allBookings.push_back(b);

    saveData();

    cout << "\n=========================================\n";
    cout << "   PAYMENT SUCCESSFUL!\n";
    cout << "   YOUR BOOKING ID: " << b.bookingID << "\n";
    cout << "   Name: " << finalName << "\n";
    cout << "=========================================\n";
    cout << "Please keep this ID for cancellation/reference.\n";

    clearCart();
}

void feedback() {
    cout << "\n-- Feedback --\n";
    int rating;
    cout << "Rate us (1-5): ";
    if (!(cin >> rating)) { clearBadInput(); cout << "Invalid.\n"; return; }
    if (rating < 1 || rating > 5) { cout << "Rating must be 1-5.\n"; return; }

    cout << "Comments: ";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    string comment;
    getline(cin, comment);
    comment = trimCopy(comment);

    feedbackList.push_back("Rating: " + to_string(rating) + " | " + comment);
    saveData();
    cout << "Thank you!\n";
}

void customerRefund() {
    cout << "\n-- Customer Booking Cancellation & Refund --\n";

    int targetID;
    cout << "Enter your Booking ID: ";
    if (!(cin >> targetID)) { clearBadInput(); return; }

    for (size_t i = 0; i < allBookings.size(); ++i) {
        if (allBookings[i].bookingID != targetID) continue;

        cout << "\n-- Booking Found --\n";
        cout << left << setw(14) << "Name" << ": " << allBookings[i].customerName << "\n";
        cout << left << setw(14) << "Date" << ": " << allBookings[i].dateStr << "\n";
        cout << left << setw(14) << "Total Paid" << ": RM " << fixed << setprecision(2)
            << allBookings[i].totalAmount << "\n";
        cout << left << setw(14) << "Items" << ":\n";
        for (const auto& it : allBookings[i].items) cout << " - " << it << "\n";

        char confirm;
        cout << "\nCancel and refund? (y/n): ";
        cin >> confirm;

        if (confirm == 'y' || confirm == 'Y') {

            // ================= RESTOCK HERE =================
            for (const string& line : allBookings[i].items) {
                // only restock merchandise/snacks
                if (line.rfind("Buy:", 0) != 0) continue;

                // Expected: "Buy: Product Name x N"
                string rest = trimCopy(line.substr(4)); // after "Buy:"
                size_t pos = rest.find(" x ");
                if (pos == string::npos) continue;

                string productName = trimCopy(rest.substr(0, pos));
                int qty = 0;
                try { qty = stoi(trimCopy(rest.substr(pos + 3))); }
                catch (...) { continue; }

                if (qty <= 0) continue;

                for (auto& p : inventory) {
                    if (p.name == productName) {
                        p.stock += qty;
                        break;
                    }
                }
            }
            // ================================================

            cout << ">> Refund Processed: RM " << fixed << setprecision(2)
                << allBookings[i].totalAmount << " returned.\n";

            allBookings.erase(allBookings.begin() + i);
            saveData();

            cout << ">> Booking ID " << targetID << " deleted.\n";
        }
        else {
            cout << "Cancellation aborted.\n";
        }

        return;
    }

    cout << "Sorry. Booking ID not found.\n";
}



// ================= ADMIN =================
void adminInventoryManage() {
    while (true) {
        cout << "\n--- Inventory Management ---\n";
        cout << "1. View Inventory\n";
        cout << "2. Restock Item\n";
        cout << "3. Add New Item\n";
        cout << "4. Reset to Default Inventory\n";
        cout << "5. Back\n";
        cout << "Choice: ";

        int c;
        if (!(cin >> c)) { clearBadInput(); continue; }

        if (c == 1) {
            cout << "\nID   " << left << setw(28) << "Product" << right << setw(10) << "Price" << setw(10) << "Stock\n";
            cout << "--------------------------------------------------------\n";
            for (const auto& p : inventory) {
                cout << left << setw(5) << p.id
                    << left << setw(28) << p.name
                    << right << setw(10) << fixed << setprecision(2) << p.price
                    << right << setw(10) << p.stock << "\n";
            }
            pause();
        }
        else if (c == 2) {
            int id, addQty;
            cout << "Enter Product ID to restock: ";
            if (!(cin >> id)) { clearBadInput(); continue; }

            auto it = find_if(inventory.begin(), inventory.end(), [&](const Product& p) { return p.id == id; });
            if (it == inventory.end()) { cout << "ID not found.\n"; continue; }

            cout << "Selected: " << it->name << " | Current stock: " << it->stock << "\n";
            cout << "Enter quantity to add: ";
            if (!(cin >> addQty)) { clearBadInput(); continue; }
            if (addQty <= 0) { cout << "Invalid quantity.\n"; continue; }

            it->stock += addQty;
            saveData();
            cout << "Restocked! New stock: " << it->stock << "\n";
            pause();
        }
        else if (c == 3) {
            Product p{};
            cout << "Enter new item ID: ";
            if (!(cin >> p.id)) { clearBadInput(); continue; }
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            cout << "Enter name: ";
            getline(cin, p.name);
            p.name = trimCopy(p.name);
            if (p.name.empty()) { cout << "Invalid name.\n"; continue; }

            cout << "Enter price: ";
            if (!(cin >> p.price)) { clearBadInput(); continue; }
            cout << "Enter stock: ";
            if (!(cin >> p.stock)) { clearBadInput(); continue; }
            if (p.stock < 0 || p.price < 0) { cout << "Invalid price/stock.\n"; continue; }

            if (find_if(inventory.begin(), inventory.end(), [&](const Product& x) { return x.id == p.id; }) != inventory.end()) {
                cout << "ID already exists.\n";
                continue;
            }

            inventory.push_back(p);
            cout << "Item added.\n";
            pause();
        }
        else if (c == 4) {
            loadDefaultsInventory();
            cout << "Inventory reset to defaults.\n";
            pause();
        }
        else if (c == 5) {
            return;
        }
        else {
            cout << "Invalid.\n";
        }
    }
}

void adminViewBookings() {
    cout << "\n--- View Bookings ---\n";
    cout << "1. View All Bookings\n";
    cout << "2. View By Date (DD/MM/YYYY)\n";
    cout << "Select: ";

    int viewType;
    if (!(cin >> viewType)) { clearBadInput(); return; }

    string targetDate;
    if (viewType == 2) {
        cout << "Enter Date: ";
        cin >> targetDate;
        targetDate = trimCopy(targetDate);
    }

    bool foundAny = false;

    for (const auto& b : allBookings) {
        bool ok = (viewType == 1);
        if (viewType == 2) {
            if (b.dateStr.size() >= 10 && b.dateStr.substr(0, 10) == targetDate) ok = true;
        }

        if (!ok) continue;
        foundAny = true;

        cout << "\nID: " << b.bookingID << " | Date: " << b.dateStr
            << " | " << b.customerName << " (" << b.customerPhone << ") | RM "
            << fixed << setprecision(2) << b.totalAmount << "\n";

        map<string, int> counts;
        for (const auto& item : b.items) counts[item]++;

        for (const auto& kv : counts) {
            cout << "   > " << kv.first;
            if (kv.second > 1) cout << " x " << kv.second;
            cout << "\n";
        }
        cout << "------------------------------\n";
    }

    if (!foundAny) {
        cout << "No bookings found for that view.\n";
    }
}

void adminStaffLogin() {
    cout << "\n-- Admin/Staff Login --\n";
    string pass;
    cout << "Enter Admin Password: ";
    cin >> pass;

    if (pass != "1234") {
        cout << "Wrong password.\n";
        return;
    }

    int adminChoice;
    do {
        cout << "\n[ADMIN MENU]\n";
        cout << "1. View Members\n";
        cout << "2. Remove Member\n";
        cout << "3. Manage Inventory\n";
        cout << "4. View Feedbacks\n";
        cout << "5. View Bookings\n";
        cout << "6. Remove Booking\n";
        cout << "7. Daily/Monthly Report\n";
        cout << "8. Logout\nChoice: ";

        if (!(cin >> adminChoice)) { clearBadInput(); continue; }

        if (adminChoice == 1) {
            cout << "\n--- Member List ---\n";
            if (members.empty()) cout << "No members found.\n";
            for (const auto& m : members) {
                cout << "Name: " << m.name << " (" << m.phone << ")\n";
                cout << " - Type  : " << m.type << "\n";
                cout << " - Join  : " << m.joinDate << "\n";
                cout << " - Expiry: " << m.expiryDate << "\n\n";
            }
            pause();
        }
        else if (adminChoice == 2) {
            string targetPhone;
            cout << "Enter Member Phone Number to Remove: ";
            cin >> targetPhone;
            bool found = false;

            for (size_t i = 0; i < members.size(); ++i) {
                if (members[i].phone == targetPhone) {
                    cout << "Removing member: " << members[i].name << "...\n";
                    members.erase(members.begin() + i);
                    saveData();
                    cout << "Member deleted successfully.\n";
                    found = true;
                    break;
                }
            }
            if (!found) cout << "Member phone not found.\n";
            pause();
        }
        else if (adminChoice == 3) {
            adminInventoryManage();
        }
        else if (adminChoice == 4) {
            cout << "\n--- Feedbacks ---\n";
            if (feedbackList.empty()) cout << "(No feedback yet)\n";
            for (const auto& f : feedbackList) cout << f << "\n";
            pause();
        }
        else if (adminChoice == 5) {
            adminViewBookings();
            pause();
        }
        else if (adminChoice == 6) {
            int idToDelete;
            cout << "Enter Booking ID to remove: ";
            cin >> idToDelete;
            bool deleted = false;

            for (size_t i = 0; i < allBookings.size(); ++i) {
                if (allBookings[i].bookingID == idToDelete) {
                    cout << "Removing booking for " << allBookings[i].customerName << "...\n";
                    allBookings.erase(allBookings.begin() + i);
                    saveData();
                    cout << "Success.\n";
                    deleted = true;
                    break;
                }
            }
            if (!deleted) cout << "ID not found.\n";
            pause();
        }
        else if (adminChoice == 7) {
            int reportChoice;
            cout << "\n-- Admin Report --\n";
            cout << "1. Daily Report\n2. Monthly Report\nChoice: ";
            cin >> reportChoice;

            if (reportChoice == 1) {
                generateDailyReports();
                checkDailyReport();
            }
            else if (reportChoice == 2) {
                generateMonthlyReports();
                checkMonthlyReport();
            }
            else {
                cout << "Invalid.\n";
            }
            pause();
        }

    } while (adminChoice != 8);
}

// ================= MAIN =================
int main() {
    loadData();
    displayIntro();

    int choice;
    do {
        string timeNow = getCurrentTimestamp(true);

        cout << "\n=============================================\n";
        cout << "      RSW SPORTS COMPLEX POS SYSTEM         \n";
        cout << "       " << timeNow << "\n";
        cout << "=============================================\n";
        cout << "  -- SERVICES --\n";
        cout << "  [1] Apply Membership\n";
        cout << "  [2] Facility Info\n";
        cout << "  [3] Book Facility\n";
        cout << "  [4] Rent Equipment\n";
        cout << "  [5] Buy Equipment & Snacks\n";
        cout << "  [6] Locker Rental\n";
        cout << "\n  -- TRANSACTION --\n";
        cout << "  [7] Checkout & Payment\n";
        cout << "  [8] Customer Refund / Cancel Booking\n";
        cout << "  [9] Clear Cart\n";
        cout << "\n  -- SYSTEM --\n";
        cout << "  [10] Feedback\n";
        cout << "  [11] Admin Login\n";
        cout << "  [12] Exit (Save & Close)\n";
        cout << "=============================================\n";
        cout << "  BILL: RM " << fixed << setprecision(2) << currentCartTotal << "\n";
        cout << "  Choice: ";

        if (!(cin >> choice)) { clearBadInput(); continue; }

        switch (choice) {
        case 1: applyMembership(); pause(); break;
        case 2: facilityInfo(); pause(); break;
        case 3: bookFacility(); pause(); break;
        case 4: rentEquipment(); pause(); break;
        case 5: buyMerchandiseSnacks(); pause(); break;
        case 6: lockerRental(); pause(); break;
        case 7: checkoutPayment(); pause(); break;
        case 8: customerRefund(); pause(); break;
        case 9: clearCart(); pause(); break;
        case 10: feedback(); pause(); break;
        case 11: adminStaffLogin(); break;
        case 12: saveData(); cout << "Exiting...\n"; break;
        default: cout << "Invalid choice.\n"; pause(); break;
        }

    } while (choice != 12);

    return 0;
}
