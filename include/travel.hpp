#pragma once

#include <string>
#include <vector>

/**
 * Represents a scheduled flight departure option with pricing and seat inventory.
 */
struct TripOption {
    std::string id;               // Unique flight identifier (e.g., "NS-204")
    std::string origin;           // 3-letter IATA origin code (e.g., "NYC")
    std::string destination;      // 3-letter IATA destination code (e.g., "SFO")
    std::string origin_name;      // Full city name for origin
    std::string destination_name; // Full city name for destination
    std::string departure;        // Scheduled departure time (HH:MM)
    std::string arrival;          // Scheduled arrival time (HH:MM)
    std::string date;             // Flight date (YYYY-MM-DD)
    std::string airline;          // Operating airline name
    int duration_minutes;         // Total flight duration in minutes
    int price_cents;              // Individual ticket price in cents
    int seats_available;          // Current remaining seat count
    int stops;                    // Number of intermediate stops
};

/**
 * Represents a directional flight leg connecting two airports.
 */
struct RouteEdge {
    std::string from;             // Origin airport code
    std::string to;               // Destination airport code
    int duration_minutes;         // Flight time in minutes
    int price_cents;              // Segment base fare in cents
};

/**
 * Represents a continuous time interval defined in minutes from start of day.
 */
struct TimeWindow {
    int start_minute;             // Start minute (e.g., 540 for 09:00)
    int end_minute;               // End minute (e.g., 630 for 10:30)
};

/**
 * Holds the result of a route planning query across multiple flight legs.
 */
struct RouteResult {
    std::vector<std::string> airports; // Ordered sequence of airport codes in itinerary
    int total_minutes;                 // Cumulative flight time in minutes
    int total_price_cents;             // Combined total ticket price in cents
};

// --- In-Memory Catalogs ---
const std::vector<TripOption>& tripCatalog();
const std::vector<RouteEdge>& routeCatalog();

// --- Travel Planning Service Functions ---
std::vector<TripOption> searchTrips(const std::string& origin, const std::string& destination, int max_price_cents);
std::vector<std::string> fewestLayoverRoute(const std::string& origin, const std::string& destination,
                                            const std::vector<RouteEdge>& edges);
std::vector<TimeWindow> availableWindows(int day_start, int day_end, std::vector<TimeWindow> booked);
RouteResult cheapestRoute(const std::string& origin, const std::string& destination,
                          const std::vector<RouteEdge>& edges);

// --- Booking Workflow Functions ---
int reservationTotalCents(const TripOption& trip, int travelers);
bool reserveSeats(TripOption& trip, int travelers);
