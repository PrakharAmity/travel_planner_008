#include "travel.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
static void closeSocket(SocketHandle socket) { closesocket(socket); }
static int socketError() { return WSAGetLastError(); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
static void closeSocket(SocketHandle socket) { close(socket); }
static int socketError() { return errno; }
#endif

struct Reservation {
    std::string id;
    std::string trip_id;
    std::string traveler_name;
    int travelers;
    int total_cents;
    std::string status;
};

struct SavedItinerary { std::string id; std::string title; std::string trip_id; };
static std::vector<TripOption> inventory = tripCatalog();
static std::vector<Reservation> reservations;
static std::vector<SavedItinerary> saved_itineraries = {
    {"IT-208", "Pacific long weekend", "NS-318"},
    {"IT-194", "Boston design crawl", "NS-204"}
};
static int next_reservation = 2401;
static int next_itinerary = 209;

static std::string jsonEscape(const std::string& value) {
    std::ostringstream out;
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
    return out.str();
}

static std::string jsonString(const std::string& value) { return "\"" + jsonEscape(value) + "\""; }

static std::string tripJson(const TripOption& trip) {
    std::ostringstream out;
    out << "{\"id\":" << jsonString(trip.id) << ",\"origin\":" << jsonString(trip.origin)
        << ",\"destination\":" << jsonString(trip.destination)
        << ",\"origin_name\":" << jsonString(trip.origin_name)
        << ",\"destination_name\":" << jsonString(trip.destination_name)
        << ",\"departure\":" << jsonString(trip.departure) << ",\"arrival\":" << jsonString(trip.arrival)
        << ",\"date\":" << jsonString(trip.date) << ",\"airline\":" << jsonString(trip.airline)
        << ",\"duration_minutes\":" << trip.duration_minutes << ",\"price_cents\":" << trip.price_cents
        << ",\"seats_available\":" << trip.seats_available << ",\"stops\":" << trip.stops << "}";
    return out.str();
}

static std::string reservationJson(const Reservation& reservation) {
    std::ostringstream out;
    out << "{\"id\":" << jsonString(reservation.id) << ",\"trip_id\":" << jsonString(reservation.trip_id)
        << ",\"traveler_name\":" << jsonString(reservation.traveler_name)
        << ",\"travelers\":" << reservation.travelers << ",\"total_cents\":" << reservation.total_cents
        << ",\"status\":" << jsonString(reservation.status) << "}";
    return out.str();
}

static std::string routeJson(const RouteResult& route) {
    std::ostringstream out;
    out << "{\"airports\":[";
    for (std::size_t i = 0; i < route.airports.size(); ++i) {
        if (i) out << ',';
        out << jsonString(route.airports[i]);
    }
    out << "],\"total_minutes\":" << route.total_minutes << ",\"total_price_cents\":" << route.total_price_cents << "}";
    return out.str();
}

static std::string stateJson() {
    std::ostringstream out;
    out << "{\"trips\":[";
    for (std::size_t i = 0; i < inventory.size(); ++i) { if (i) out << ','; out << tripJson(inventory[i]); }
    out << "],\"saved_itineraries\":[";
    for (std::size_t i = 0; i < saved_itineraries.size(); ++i) {
        if (i) out << ',';
        const auto& saved = saved_itineraries[i];
        out << "{\"id\":" << jsonString(saved.id) << ",\"title\":" << jsonString(saved.title)
            << ",\"trip_id\":" << jsonString(saved.trip_id) << "}";
    }
    out << "],\"reservations\":[";
    for (std::size_t i = 0; i < reservations.size(); ++i) { if (i) out << ','; out << reservationJson(reservations[i]); }
    const auto least_stops = fewestLayoverRoute("SEA", "NYC", routeCatalog());
    const auto cheapest = cheapestRoute("SEA", "NYC", routeCatalog());
    const auto open = availableWindows(8 * 60, 20 * 60, {{9 * 60, 10 * 60 + 30}, {9 * 60 + 20, 9 * 60 + 45}, {12 * 60, 13 * 60}, {16 * 60, 17 * 60 + 15}});
    out << "],\"fewest_layovers\":[";
    for (std::size_t i = 0; i < least_stops.size(); ++i) { if (i) out << ','; out << jsonString(least_stops[i]); }
    out << "],\"cheapest_route\":" << routeJson(cheapest) << ",\"open_windows\":[";
    for (std::size_t i = 0; i < open.size(); ++i) {
        if (i) out << ',';
        out << "{\"start\":" << open[i].start_minute << ",\"end\":" << open[i].end_minute << "}";
    }
    out << "]}";
    return out.str();
}

static std::string decode(const std::string& value) {
    std::string decoded;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+') decoded.push_back(' ');
        else if (value[i] == '%' && i + 2 < value.size()) {
            const std::string hex = value.substr(i + 1, 2);
            char* end = nullptr;
            const long ch = std::strtol(hex.c_str(), &end, 16);
            if (end && *end == '\0') { decoded.push_back(static_cast<char>(ch)); i += 2; }
            else decoded.push_back(value[i]);
        } else decoded.push_back(value[i]);
    }
    return decoded;
}

static std::map<std::string, std::string> parsePairs(const std::string& source, char separator) {
    std::map<std::string, std::string> pairs;
    std::istringstream stream(source);
    std::string part;
    while (std::getline(stream, part, separator)) {
        const std::size_t equals = part.find('=');
        if (equals == std::string::npos) continue;
        pairs[decode(part.substr(0, equals))] = decode(part.substr(equals + 1));
    }
    return pairs;
}

static std::string jsonField(const std::string& body, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    std::size_t at = body.find(token);
    if (at == std::string::npos) return "";
    at = body.find(':', at + token.size());
    if (at == std::string::npos) return "";
    at = body.find_first_not_of(" \t\r\n", at + 1);
    if (at == std::string::npos || body[at] != '"') return "";
    const std::size_t end = body.find('"', at + 1);
    if (end == std::string::npos) return "";
    return body.substr(at + 1, end - at - 1);
}

static int jsonInteger(const std::string& body, const std::string& key, int fallback) {
    const std::string token = "\"" + key + "\"";
    std::size_t at = body.find(token);
    if (at == std::string::npos) return fallback;
    at = body.find(':', at + token.size());
    if (at == std::string::npos) return fallback;
    return std::atoi(body.c_str() + at + 1);
}

struct Request { std::string method; std::string path; std::string body; };

static bool readRequest(SocketHandle client, Request& request) {
    std::string wire;
    char buffer[4096];
    std::size_t header_end = std::string::npos;
    std::size_t expected_body = 0;
    while ((header_end = wire.find("\r\n\r\n")) == std::string::npos) {
        const int count = recv(client, buffer, sizeof(buffer), 0);
        if (count <= 0) return false;
        wire.append(buffer, count);
    }
    const std::string headers = wire.substr(0, header_end);
    std::istringstream lines(headers);
    std::string first_line;
    std::getline(lines, first_line);
    std::istringstream first(first_line);
    first >> request.method >> request.path;
    std::string line;
    while (std::getline(lines, line)) {
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower.find("content-length:") == 0) expected_body = static_cast<std::size_t>(std::strtoul(lower.c_str() + 15, nullptr, 10));
    }
    std::size_t body_start = header_end + 4;
    while (wire.size() < body_start + expected_body) {
        const int count = recv(client, buffer, sizeof(buffer), 0);
        if (count <= 0) return false;
        wire.append(buffer, count);
    }
    request.body = wire.substr(body_start, expected_body);
    return true;
}

static void respond(SocketHandle client, int status, const std::string& content_type, const std::string& body) {
    const std::string status_text = status == 200 ? "OK" : status == 201 ? "Created" : status == 400 ? "Bad Request" : status == 404 ? "Not Found" : status == 409 ? "Conflict" : "Internal Server Error";
    std::ostringstream header;
    header << "HTTP/1.1 " << status << ' ' << status_text << "\r\nContent-Type: " << content_type
           << "\r\nContent-Length: " << body.size() << "\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
    const std::string payload = header.str() + body;
    std::size_t sent = 0;
    while (sent < payload.size()) {
        const int count = send(client, payload.data() + sent, static_cast<int>(payload.size() - sent), 0);
        if (count <= 0) break;
        sent += static_cast<std::size_t>(count);
    }
}

static std::string readFile(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) return "";
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

static void handleRequest(SocketHandle client, const Request& request) {
    const std::size_t question = request.path.find('?');
    const std::string path = request.path.substr(0, question);
    const auto query = question == std::string::npos ? std::map<std::string, std::string>() : parsePairs(request.path.substr(question + 1), '&');
    if (request.method == "GET" && (path == "/" || path == "/index.html")) {
        const std::string html = readFile("public/index.html");
        respond(client, html.empty() ? 404 : 200, "text/html; charset=utf-8", html.empty() ? "Not found" : html);
        return;
    }
    if (request.method == "GET" && (path == "/styles.css" || path == "/app.js")) {
        const std::string filename = path == "/styles.css" ? "public/styles.css" : "public/app.js";
        const std::string content = readFile(filename);
        respond(client, content.empty() ? 404 : 200, path == "/styles.css" ? "text/css; charset=utf-8" : "application/javascript; charset=utf-8", content);
        return;
    }
    if (request.method == "GET" && path == "/api/state") {
        respond(client, 200, "application/json; charset=utf-8", stateJson());
        return;
    }
    if (request.method == "GET" && path == "/api/search") {
        const std::string origin = query.count("origin") ? query.at("origin") : "NYC";
        const std::string destination = query.count("destination") ? query.at("destination") : "SFO";
        const int max_price = query.count("maxPrice") ? std::atoi(query.at("maxPrice").c_str()) : 1000000;
        auto trips = searchTrips(origin, destination, max_price);
        trips.erase(std::remove_if(trips.begin(), trips.end(), [&](TripOption& trip) {
            const auto live = std::find_if(inventory.begin(), inventory.end(), [&](const TripOption& candidate) { return candidate.id == trip.id; });
            if (live == inventory.end() || live->seats_available <= 0) return true;
            trip.seats_available = live->seats_available;
            return false;
        }), trips.end());
        std::ostringstream result;
        result << "{\"trips\":[";
        for (std::size_t i = 0; i < trips.size(); ++i) { if (i) result << ','; result << tripJson(trips[i]); }
        result << "]}";
        respond(client, 200, "application/json; charset=utf-8", result.str());
        return;
    }
    if (request.method == "POST" && path == "/api/reservations") {
        const std::string trip_id = jsonField(request.body, "trip_id");
        const std::string name = jsonField(request.body, "traveler_name");
        const int travelers = jsonInteger(request.body, "travelers", 1);
        auto found = std::find_if(inventory.begin(), inventory.end(), [&](const TripOption& trip) { return trip.id == trip_id; });
        if (found == inventory.end() || travelers < 1) {
            respond(client, 400, "application/json; charset=utf-8", "{\"error\":\"Choose a valid departure and traveler count.\"}");
            return;
        }
        if (!reserveSeats(*found, travelers)) {
            respond(client, 409, "application/json; charset=utf-8", "{\"error\":\"Not enough seats remain on this departure.\"}");
            return;
        }
        Reservation reservation{"RS-" + std::to_string(next_reservation++), trip_id, name.empty() ? "Traveler" : name,
                                travelers, reservationTotalCents(*found, travelers), "confirmed"};
        reservations.push_back(reservation);
        respond(client, 201, "application/json; charset=utf-8", reservationJson(reservation));
        return;
    }
    if (request.method == "PATCH" && path.find("/api/reservations/") == 0) {
        const std::string reservation_id = path.substr(std::string("/api/reservations/").size());
        auto reservation = std::find_if(reservations.begin(), reservations.end(), [&](const Reservation& item) { return item.id == reservation_id; });
        if (reservation == reservations.end()) {
            respond(client, 404, "application/json; charset=utf-8", "{\"error\":\"Reservation not found.\"}");
            return;
        }
        if (reservation->status != "confirmed" || jsonField(request.body, "status") != "cancelled") {
            respond(client, 409, "application/json; charset=utf-8", "{\"error\":\"This reservation cannot be changed.\"}");
            return;
        }
        const auto trip = std::find_if(inventory.begin(), inventory.end(), [&](const TripOption& item) { return item.id == reservation->trip_id; });
        if (trip != inventory.end()) trip->seats_available += reservation->travelers;
        reservation->status = "cancelled";
        respond(client, 200, "application/json; charset=utf-8", reservationJson(*reservation));
        return;
    }
    if (request.method == "POST" && path == "/api/itineraries") {
        const std::string title = jsonField(request.body, "title");
        const std::string trip_id = jsonField(request.body, "trip_id");
        if (title.empty() || trip_id.empty()) {
            respond(client, 400, "application/json; charset=utf-8", "{\"error\":\"Add a trip and itinerary name.\"}");
            return;
        }
        SavedItinerary saved{"IT-" + std::to_string(next_itinerary++), title, trip_id};
        saved_itineraries.insert(saved_itineraries.begin(), saved);
        std::ostringstream result;
        result << "{\"id\":" << jsonString(saved.id) << ",\"title\":" << jsonString(saved.title) << ",\"trip_id\":" << jsonString(saved.trip_id) << "}";
        respond(client, 201, "application/json; charset=utf-8", result.str());
        return;
    }
    if (request.method == "OPTIONS") {
        respond(client, 200, "text/plain", "");
        return;
    }
    respond(client, 404, "application/json; charset=utf-8", "{\"error\":\"Route not found.\"}");
}

int main() {
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) { std::cerr << "Could not initialize sockets\n"; return 1; }
#endif
    const char* configured_port = std::getenv("PORT");
    const int port = configured_port ? std::atoi(configured_port) : 8080;
    SocketHandle listener = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (listener == INVALID_SOCKET) { std::cerr << "socket failed: " << socketError() << '\n'; WSACleanup(); return 1; }
    BOOL reuse = TRUE;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
    if (listener < 0) { std::cerr << "socket failed: " << socketError() << '\n'; return 1; }
    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<unsigned short>(port));
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(listener, 32) != 0) {
        std::cerr << "Could not bind to 0.0.0.0:" << port << " (" << socketError() << ")\n";
        closeSocket(listener);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    std::cerr << "Northstar listening on 0.0.0.0:" << port << '\n';
    while (true) {
        sockaddr_in peer{};
#ifdef _WIN32
        int peer_size = sizeof(peer);
#else
        socklen_t peer_size = sizeof(peer);
#endif
        SocketHandle client = accept(listener, reinterpret_cast<sockaddr*>(&peer), &peer_size);
#ifdef _WIN32
        if (client == INVALID_SOCKET) continue;
#else
        if (client < 0) continue;
#endif
        Request request;
        if (readRequest(client, request)) handleRequest(client, request);
        closeSocket(client);
    }
    closeSocket(listener);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
