# Northstar Travel Planner — AI Context & Bug Reference

## 1. Project Overview

Northstar is a lightweight travel planning service written in C++14. It exposes an embedded HTTP server serving a single-page web application from `public/` along with REST endpoints for flight search, route planning, schedule window calculation, and in-memory reservations.

---

## 2. Repository Structure

```
travel-planner-cpp/
├── CMakeLists.txt         # CMake build configuration for library, server, and test runner
├── README.md              # Project documentation and challenge brief
├── AI.md                  # Architecture, bug catalog, and developer summary
├── challenge.json         # Platform challenge configuration and test commands
├── start.sh               # Bash live-reload server runner
├── include/
│   └── travel.hpp         # Domain data structures (TripOption, RouteEdge, TimeWindow, RouteResult)
│                          # and service function declarations
├── src/
│   ├── main.cpp           # Embedded HTTP server (WinSock2/POSIX) and REST routing
│   └── service.cpp        # [BUG LOCATION] Core algorithms & domain business logic
├── public/
│   ├── index.html         # Web frontend markup
│   ├── styles.css         # UI stylesheet
│   └── app.js             # Client application logic
└── tests/
    ├── test_runner.cpp    # Test suite executable and JSON telemetry reporter
    └── run_tests.sh       # Bash script to build and execute test suite
```

---

## 3. Bug Locations & Summary

All 6 challenge bugs reside exclusively in:
> **Target File:** [`src/service.cpp`](file:///c:/Users/prakh/Documents/Codex/2026-09-28/you-are-an-expert-application-engineer/outputs/travel-planner-cpp/src/service.cpp)  
> **Verification Suite:** [`tests/test_runner.cpp`](file:///c:/Users/prakh/Documents/Codex/2026-09-28/you-are-an-expert-application-engineer/outputs/travel-planner-cpp/tests/test_runner.cpp)

---

## 4. Detailed Bug Breakdown

### Bug 1: Departure Search Catalog Filter
- **File & Function:** `src/service.cpp` → `searchTrips()`
- **Issue:** Catalog entries are not pre-sorted by destination. Using `std::lower_bound` assumed sorted order, prematurely terminating the search and omitting valid matching flights.
- **Fix:** Iterate through the catalog linearly to collect all flights matching `origin`, `destination`, price threshold (`price_cents <= max_price_cents`), and available seats (`seats_available > 0`).

---

### Bug 2: Connection Planner Layover Minimization
- **File & Function:** `src/service.cpp` → `fewestLayoverRoute()`
- **Issue:** Used depth-first search (DFS), which returned the first path explored rather than the path with the minimum number of flight legs/layovers.
- **Fix:** Implemented breadth-first search (BFS) with queue and parent tracking to guarantee discovering the path with the fewest hops.

---

### Bug 3: Flexible Windows Nested Booking Merge
- **File & Function:** `src/service.cpp` → `availableWindows()`
- **Issue:** When merging overlapping time intervals, assigning `merged.back().end_minute = end` caused nested intervals (which end earlier) to shrink the busy window rather than maintaining the maximum end boundary.
- **Fix:** Used `std::max(merged.back().end_minute, end)` when merging overlapping intervals.

---

### Bug 4: Fare Finder Lowest Total Price
- **File & Function:** `src/service.cpp` → `cheapestRoute()`
- **Issue:** Used unweighted FIFO queue traversal (BFS), finding the connection with the fewest legs ($360) instead of the lowest cumulative ticket price ($270).
- **Fix:** Implemented a priority queue (min-heap ordered by total fare) to find the route with the minimum accumulated price.

---

### Bug 5: Group Pricing Reservation Total Scaling
- **File & Function:** `src/service.cpp` → `reservationTotalCents()`
- **Issue:** The traveler count was ignored (`(void)travelers;`), charging only for a single traveler regardless of party size.
- **Fix:** Multiplied single ticket price by party size: `trip.price_cents * travelers`.

---

### Bug 6: Seat Inventory Limit Boundary Check
- **File & Function:** `src/service.cpp` → `reserveSeats()`
- **Issue:** The check did not verify if `travelers > trip.seats_available`, allowing reservations to exceed remaining seats and decrementing availability into negative values.
- **Fix:** Added a boundary guard `if (travelers < 1 || travelers > trip.seats_available) return false;` without modifying inventory.

