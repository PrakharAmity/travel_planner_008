#pragma once

#include <string>
#include <vector>

struct TripOption {
    std::string id;
    std::string origin;
    std::string destination;
    std::string origin_name;
    std::string destination_name;
    std::string departure;
    std::string arrival;
    std::string date;
    std::string airline;
    int duration_minutes;
    int price_cents;
    int seats_available;
    int stops;
};

struct RouteEdge {
    std::string from;
    std::string to;
    int duration_minutes;
    int price_cents;
};

struct TimeWindow {
    int start_minute;
    int end_minute;
};

struct RouteResult {
    std::vector<std::string> airports;
    int total_minutes;
    int total_price_cents;
};

const std::vector<TripOption>& tripCatalog();
const std::vector<RouteEdge>& routeCatalog();
std::vector<TripOption> searchTrips(const std::string& origin, const std::string& destination, int max_price_cents);
std::vector<std::string> fewestLayoverRoute(const std::string& origin, const std::string& destination,
                                            const std::vector<RouteEdge>& edges);
std::vector<TimeWindow> availableWindows(int day_start, int day_end, std::vector<TimeWindow> booked);
RouteResult cheapestRoute(const std::string& origin, const std::string& destination,
                          const std::vector<RouteEdge>& edges);
int reservationTotalCents(const TripOption& trip, int travelers);
bool reserveSeats(TripOption& trip, int travelers);
