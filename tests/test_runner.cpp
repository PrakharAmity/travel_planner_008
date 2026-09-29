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
        {"test_search_returns_all_matching_departures", [] {
            const auto found = searchTrips("NYC", "SFO", 45000);
            expect(found.size() == 3, "Expected 3 New York to San Francisco departures under $450, got " + std::to_string(found.size()));
        }},
        {"test_connections_minimize_layovers", [] {
            const auto route = fewestLayoverRoute("SEA", "NYC", routeCatalog());
            const std::vector<std::string> expected = {"SEA", "SFO", "NYC"};
            expect(route == expected, "Expected fewest-layover route SEA → SFO → NYC");
        }},
        {"test_calendar_merges_nested_booking_windows", [] {
            const auto free = availableWindows(0, 60, {{10, 40}, {15, 20}});
            expect(free.size() == 2 && free[0].start_minute == 0 && free[0].end_minute == 10 &&
                   free[1].start_minute == 40 && free[1].end_minute == 60,
                   "Expected open windows 00:00–00:10 and 00:40–01:00 after overlapping bookings");
        }},
        {"test_fare_finder_selects_lowest_total_price", [] {
            const auto route = cheapestRoute("SEA", "NYC", routeCatalog());
            const std::vector<std::string> expected = {"SEA", "DEN", "DFW", "NYC"};
            expect(route.airports == expected && route.total_price_cents == 27000,
                   "Expected lowest fare $270 via SEA → DEN → DFW → NYC, got $" + std::to_string(route.total_price_cents / 100));
        }},
        {"test_reservation_total_scales_with_travelers", [] {
            const auto& trip = tripCatalog()[1];
            const int total = reservationTotalCents(trip, 3);
            expect(total == 116700, "Expected 3 traveler total $1167, got $" + std::to_string(total / 100));
        }},
        {"test_booking_rejects_requests_over_seat_inventory", [] {
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
    std::cout << "{";
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        passed += result.passed ? 1 : 0;
        failed += result.passed ? 0 : 1;
        total_ms += result.elapsed_ms;
        if (i) std::cout << ",";
        std::cout << quoteJson(result.name) << ":{\"Status\":" << quoteJson(result.passed ? "passed" : "failed")
                  << ",\"Execution time\":" << quoteJson(std::to_string(result.elapsed_ms) + "ms");
        if (!result.passed) std::cout << ",\"Error\":" << quoteJson(result.error);
        std::cout << "}";
    }
    std::cout << ",\"Passed\":" << passed << ",\"Failed\":" << failed << ",\"Total bugs\":" << tests.size()
              << ",\"Total Execution time\":" << quoteJson(std::to_string(total_ms) + "ms") << "}\n";
    return failed == 0 ? 0 : 1;
}
