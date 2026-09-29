#include "travel.hpp"

#include <chrono>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct TestCase { std::string name; std::function<void()> run; };
struct TestResult { std::string name; bool passed; long long elapsed_ms; std::string error; };

static std::string quoteJson(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (unsigned char ch : value) {
        switch (ch) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: if (ch >= 0x20) out << ch;
        }
    }
    out << '"';
    return out.str();
}

static void expect(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    const std::vector<TestCase> tests = {
        {"Bug 1: Departure Search Catalog Filter", [] {
            const auto found = searchTrips("NYC", "SFO", 45000);
            expect(found.size() == 3, "Expected 3 New York to San Francisco departures under $450, got " + std::to_string(found.size()));
        }},
        {"Bug 2: Connection Planner Layover Minimization", [] {
            const auto route = fewestLayoverRoute("SEA", "NYC", routeCatalog());
            const std::vector<std::string> expected = {"SEA", "SFO", "NYC"};
            expect(route == expected, "Expected fewest-layover route SEA → SFO → NYC");
        }},
        {"Bug 3: Flexible Windows Nested Booking Merge", [] {
            const auto free = availableWindows(0, 60, {{10, 40}, {15, 20}});
            expect(free.size() == 2 && free[0].start_minute == 0 && free[0].end_minute == 10 &&
                   free[1].start_minute == 40 && free[1].end_minute == 60,
                   "Expected open windows 00:00–00:10 and 00:40–01:00 after overlapping bookings");
        }},
        {"Bug 4: Fare Finder Lowest Total Price", [] {
            const auto route = cheapestRoute("SEA", "NYC", routeCatalog());
            const std::vector<std::string> expected = {"SEA", "DEN", "DFW", "NYC"};
            expect(route.airports == expected && route.total_price_cents == 27000,
                   "Expected lowest fare $270 via SEA → DEN → DFW → NYC, got $" + std::to_string(route.total_price_cents / 100));
        }},
        {"Bug 5: Group Pricing Reservation Total Scaling", [] {
            const auto& trip = tripCatalog()[1];
            const int total = reservationTotalCents(trip, 3);
            expect(total == 116700, "Expected 3 traveler total $1167, got $" + std::to_string(total / 100));
        }},
        {"Bug 6: Seat Inventory Limit Boundary Check", [] {
            TripOption trip = tripCatalog()[0];
            trip.seats_available = 2;
            const bool booked = reserveSeats(trip, 3);
            expect(!booked && trip.seats_available == 2, "Expected a 3 traveler booking to be rejected with 2 seats remaining");
        }}
    };

    std::vector<TestResult> results;
    for (const auto& test : tests) {
        const auto start = std::chrono::steady_clock::now();
        TestResult result{test.name, true, 0, ""};
        try { test.run(); }
        catch (const std::exception& error) { result.passed = false; result.error = error.what(); }
        catch (...) { result.passed = false; result.error = "Unexpected test error"; }
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        results.push_back(result);
    }

    int passed = 0;
    int failed = 0;
    long long total_ms = 0;
    std::cout << "{\n";
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        passed += result.passed ? 1 : 0;
        failed += result.passed ? 0 : 1;
        total_ms += result.elapsed_ms;
        if (i) std::cout << ",\n";
        std::cout << "  " << quoteJson(result.name) << ": {\n"
                  << "    \"Status\": " << quoteJson(result.passed ? "passed" : "failed") << ",\n"
                  << "    \"Execution time\": " << quoteJson(std::to_string(result.elapsed_ms) + "ms") << "\n"
                  << "  }";
    }
    std::cout << ",\n  \"Total bugs\": " << tests.size() << ",\n"
              << "  \"Passed\": " << passed << ",\n"
              << "  \"Failed\": " << failed << ",\n"
              << "  \"Total Execution time\": " << quoteJson(std::to_string(total_ms) + "ms") << "\n}\n";
    return failed == 0 ? 0 : 1;
}
