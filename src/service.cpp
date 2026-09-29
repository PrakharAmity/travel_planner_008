#include "travel.hpp"

#include <algorithm>
#include <climits>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <utility>
#include <vector>

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

const std::vector<RouteEdge>& routeCatalog() {
    static const std::vector<RouteEdge> edges = {
        {"SEA", "DEN", 145, 8000}, {"DEN", "DFW", 115, 10000}, {"DFW", "NYC", 195, 9000},
        {"SEA", "SFO", 125, 22000}, {"SFO", "NYC", 335, 14000}, {"SEA", "LAX", 165, 12500},
        {"LAX", "NYC", 300, 15000}, {"DEN", "ORD", 150, 11000}, {"ORD", "NYC", 120, 8000}
    };
    return edges;
}

std::vector<TripOption> searchTrips(const std::string& origin, const std::string& destination, int max_price_cents) {
    const auto& trips = tripCatalog();
    std::vector<TripOption> matches;

    for (const auto& trip : trips) {
        if (trip.origin == origin &&
            trip.destination == destination &&
            trip.price_cents <= max_price_cents &&
            trip.seats_available > 0) {
            matches.push_back(trip);
        }
    }

    return matches;
}

std::vector<std::string> fewestLayoverRoute(const std::string& origin, const std::string& destination,
                                            const std::vector<RouteEdge>& edges) {
    if (origin == destination) {
        return {origin};
    }

    std::queue<std::string> pending;
    std::set<std::string> visited;
    std::map<std::string, std::string> parent;

    pending.push(origin);
    visited.insert(origin);

    bool target_reached = false;

    while (!pending.empty()) {
        const std::string current = pending.front();
        pending.pop();

        if (current == destination) {
            target_reached = true;
            break;
        }

        for (const auto& edge : edges) {
            if (edge.from == current && !visited.count(edge.to)) {
                visited.insert(edge.to);
                parent[edge.to] = current;
                pending.push(edge.to);
            }
        }
    }

    if (!target_reached) {
        return {};
    }

    std::vector<std::string> path;
    for (std::string node = destination; !node.empty(); node = (node == origin ? "" : parent[node])) {
        path.push_back(node);
    }

    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<TimeWindow> availableWindows(int day_start, int day_end, std::vector<TimeWindow> booked) {
    std::sort(booked.begin(), booked.end(), [](const TimeWindow& a, const TimeWindow& b) {
        return a.start_minute < b.start_minute;
    });

    std::vector<TimeWindow> merged;

    for (const auto& window : booked) {
        if (window.end_minute <= day_start || window.start_minute >= day_end) {
            continue;
        }

        const int start = std::max(day_start, window.start_minute);
        const int end = std::min(day_end, window.end_minute);

        if (merged.empty() || start > merged.back().end_minute) {
            merged.push_back({start, end});
        } else {
            merged.back().end_minute = std::max(merged.back().end_minute, end);
        }
    }

    std::vector<TimeWindow> free_windows;
    int cursor = day_start;

    for (const auto& window : merged) {
        if (window.start_minute > cursor) {
            free_windows.push_back({cursor, window.start_minute});
        }
        cursor = std::max(cursor, window.end_minute);
    }

    if (cursor < day_end) {
        free_windows.push_back({cursor, day_end});
    }

    return free_windows;
}

RouteResult cheapestRoute(const std::string& origin, const std::string& destination,
                          const std::vector<RouteEdge>& edges) {
    if (origin == destination) {
        return {{origin}, 0, 0};
    }

    using QueueEntry = std::pair<int, std::string>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> pq;

    std::map<std::string, int> min_fare;
    std::map<std::string, int> duration;
    std::map<std::string, std::string> parent;

    min_fare[origin] = 0;
    duration[origin] = 0;
    pq.push({0, origin});

    while (!pq.empty()) {
        const auto top = pq.top();
        pq.pop();

        const int current_fare = top.first;
        const std::string current_city = top.second;

        if (current_fare > min_fare[current_city]) {
            continue;
        }

        if (current_city == destination) {
            break;
        }

        for (const auto& edge : edges) {
            if (edge.from == current_city) {
                const int next_fare = current_fare + edge.price_cents;

                if (!min_fare.count(edge.to) || next_fare < min_fare[edge.to]) {
                    min_fare[edge.to] = next_fare;
                    duration[edge.to] = duration[current_city] + edge.duration_minutes;
                    parent[edge.to] = current_city;
                    pq.push({next_fare, edge.to});
                }
            }
        }
    }

    if (!min_fare.count(destination)) {
        return {{}, 0, 0};
    }

    std::vector<std::string> path;
    for (std::string node = destination; !node.empty(); node = (node == origin ? "" : parent[node])) {
        path.push_back(node);
    }

    std::reverse(path.begin(), path.end());
    return {path, duration[destination], min_fare[destination]};
}

int reservationTotalCents(const TripOption& trip, int travelers) {
    return trip.price_cents * travelers;
}

bool reserveSeats(TripOption& trip, int travelers) {
    if (travelers < 1 || travelers > trip.seats_available) {
        return false;
    }

    trip.seats_available -= travelers;
    return true;
}
