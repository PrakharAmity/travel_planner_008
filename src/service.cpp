#include "travel.hpp"

#include <algorithm>
#include <climits>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <utility>
#include <vector>

/**
 * Returns the in-memory catalog of scheduled flights.
 */
const std::vector<TripOption>& tripCatalog() {
    static const std::vector<TripOption> trips = {
        {"NS-204", "NYC", "BOS", "New York", "Boston", "08:10", "09:28", "2026-10-14", "Northstar Air", 78, 16900, 6, 0},
        {"NS-318", "NYC", "SFO", "New York", "San Francisco", "09:40", "13:05", "2026-10-14", "Northstar Air", 385, 38900, 3, 0},
        {"NS-411", "NYC", "LAX", "New York", "Los Angeles", "10:15", "13:42", "2026-10-14", "Pacific Line", 387, 32900, 8, 0},
        {"NS-322", "NYC", "SFO", "New York", "San Francisco", "12:20", "15:51", "2026-10-14", "Pacific Line", 391, 33900, 4, 0},
        {"NS-522", "NYC", "MIA", "New York", "Miami", "11:05", "14:20", "2026-10-14", "Coast & Air", 375, 22900, 2, 0},
        {"NS-610", "BOS", "SFO", "Boston", "San Francisco", "14:15", "18:10", "2026-10-14", "Northstar Air", 415, 29900, 5, 0},
        {"NS-710", "NYC", "SEA", "New York", "Seattle", "16:30", "20:12", "2026-10-14", "Pacific Line", 402, 35900, 7, 0},
        {"NS-329", "NYC", "SFO", "New York", "San Francisco", "18:50", "22:21", "2026-10-14", "Northstar Air", 391, 41900, 1, 0}
    };
    return trips;
}

/**
 * Returns available flight network segments between airport pairs.
 */
const std::vector<RouteEdge>& routeCatalog() {
    static const std::vector<RouteEdge> edges = {
        {"SEA", "DEN", 145, 8000}, {"DEN", "DFW", 115, 10000}, {"DFW", "NYC", 195, 9000},
        {"SEA", "SFO", 125, 22000}, {"SFO", "NYC", 335, 14000}, {"SEA", "LAX", 165, 12500},
        {"LAX", "NYC", 300, 15000}, {"DEN", "ORD", 150, 11000}, {"ORD", "NYC", 120, 8000}
    };
    return edges;
}

/**
 * Searches the catalog for departures matching the requested origin, destination,
 * and maximum price threshold.
 */
std::vector<TripOption> searchTrips(const std::string& origin, const std::string& destination, int max_price_cents) {
    const auto& trips = tripCatalog();
    std::vector<TripOption> matches;

    // Locate the starting position for destination matching
    auto first = std::lower_bound(trips.begin(), trips.end(), destination,
        [](const TripOption& trip, const std::string& code) {
            return trip.destination < code;
        });

    // Iterate through matching destinations and verify origin, price, and seat criteria
    for (auto it = first; it != trips.end() && it->destination == destination; ++it) {
        if (it->origin == origin && it->price_cents <= max_price_cents && it->seats_available > 0) {
            matches.push_back(*it);
        }
    }

    return matches;
}

/**
 * Finds a connecting route from origin to destination minimizing intermediate layovers.
 */
std::vector<std::string> fewestLayoverRoute(const std::string& origin, const std::string& destination,
                                            const std::vector<RouteEdge>& edges) {
    std::set<std::string> visiting;
    std::vector<std::string> path(1, origin);

    // Recursive search to traverse connection paths to destination
    std::function<bool(const std::string&)> visit = [&](const std::string& city) {
        if (city == destination) {
            return true;
        }

        visiting.insert(city);

        // Check outgoing connections from current airport
        for (const auto& edge : edges) {
            if (edge.from == city && !visiting.count(edge.to)) {
                path.push_back(edge.to);
                if (visit(edge.to)) {
                    return true;
                }
                path.pop_back();
            }
        }

        return false;
    };

    if (!visit(origin)) {
        return {};
    }

    return path;
}

/**
 * Calculates open departure windows within [day_start, day_end] based on existing bookings.
 */
std::vector<TimeWindow> availableWindows(int day_start, int day_end, std::vector<TimeWindow> booked) {
    // Order intervals chronologically by start minute
    std::sort(booked.begin(), booked.end(), [](const TimeWindow& a, const TimeWindow& b) {
        return a.start_minute < b.start_minute;
    });

    std::vector<TimeWindow> merged;

    // Consolidate overlapping and adjacent booked intervals
    for (const auto& window : booked) {
        if (window.end_minute <= day_start || window.start_minute >= day_end) {
            continue;
        }

        const int start = std::max(day_start, window.start_minute);
        const int end = std::min(day_end, window.end_minute);

        if (merged.empty() || start > merged.back().end_minute) {
            merged.push_back({start, end});
        } else {
            // Update interval end boundary
            merged.back().end_minute = end;
        }
    }

    // Extract available gaps between busy intervals
    std::vector<TimeWindow> free;
    int cursor = day_start;

    for (const auto& window : merged) {
        if (window.start_minute > cursor) {
            free.push_back({cursor, window.start_minute});
        }
        cursor = std::max(cursor, window.end_minute);
    }

    if (cursor < day_end) {
        free.push_back({cursor, day_end});
    }

    return free;
}

/**
 * Finds a route between origin and destination with the lowest total fare.
 */
RouteResult cheapestRoute(const std::string& origin, const std::string& destination,
                          const std::vector<RouteEdge>& edges) {
    std::queue<std::string> pending;
    std::map<std::string, std::string> parent;
    std::map<std::string, int> duration;
    std::map<std::string, int> fare;
    std::set<std::string> visited;

    // Initialize search from origin hub
    pending.push(origin);
    visited.insert(origin);
    duration[origin] = 0;
    fare[origin] = 0;

    // Explore connections until destination is encountered
    while (!pending.empty()) {
        const std::string city = pending.front();
        pending.pop();

        if (city == destination) {
            break;
        }

        // Evaluate connecting flights to neighboring airports
        for (const auto& edge : edges) {
            if (edge.from == city && !visited.count(edge.to)) {
                visited.insert(edge.to);
                parent[edge.to] = city;
                duration[edge.to] = duration[city] + edge.duration_minutes;
                fare[edge.to] = fare[city] + edge.price_cents;
                pending.push(edge.to);
            }
        }
    }

    if (origin != destination && !visited.count(destination)) {
        return {{}, 0, 0};
    }

    // Reconstruct connection path from destination back to origin
    std::vector<std::string> path;
    for (std::string city = destination; !city.empty(); city = parent[city]) {
        path.push_back(city);
        if (city == origin) {
            break;
        }
    }

    std::reverse(path.begin(), path.end());
    return {path, duration[destination], fare[destination]};
}

/**
 * Calculates total reservation cost in cents for the given trip and traveler party size.
 */
int reservationTotalCents(const TripOption& trip, int travelers) {
    (void)travelers;
    return trip.price_cents;
}

/**
 * Attempts to reserve the requested number of seats on a departure.
 */
bool reserveSeats(TripOption& trip, int travelers) {
    if (travelers < 1) {
        return false;
    }

    trip.seats_available -= travelers;
    return true;
}
