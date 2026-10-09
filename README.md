# Northstar — Travel Planner & Reservation Dashboard

Northstar is an interactive travel planning and flight reservation dashboard built for travel coordinators and passengers. It allows users to browse scheduled departures, find optimal flight connections with minimal layovers, discover lowest-fare multi-hop routes, inspect flexible travel windows, and manage reservations using in-memory inventory.

---

## 1. Application Overview

### Core Functionality
- **Departure Search**: Search and compare scheduled flights between major hubs filtered by route, departure time, and price cap.
- **Connection Planner**: Identify optimal routes between airports with minimum layovers and flight legs.
- **Fare Finder**: Discover the lowest total ticket cost across multi-hop airline routes.
- **Flexible Calendar Windows**: Inspect available departure windows after merging complex, overlapping reservations.
- **Reservation & Inventory Management**: Book flights for parties with real-time seat inventory decrements and group pricing.
- **Saved Itineraries**: Bookmark and review preferred multi-city itineraries.

### Technology Stack
- **Framework**: C++14 (native WinSock2 / POSIX embedded HTTP server)
- **Build Tool / Dev Server**: CMake (>= 3.16) with Ninja / GCC (`start.sh`, served on `0.0.0.0:8080`)
- **Styling**: Vanilla CSS (custom design system, responsive grid layout, flight cards, modal dialogs)
- **Testing**: C++ test runner (`tests/test_runner.cpp`)
- **Data**: Bundled deterministic in-memory fixtures (`src/service.cpp`)

---

## 2. Debugging Challenge

QA engineers and early users have flagged several issues in the Northstar platform. Your goal is to investigate the codebase, reproduce each bug, and implement the necessary fixes so that all automated test suites pass.

### Reported Issues & Tasks:

#### Issue 1: Boundary & Multi-Destination Departures Missing from Search
- **User Symptom**: When searching departures between an origin and destination (e.g., NYC to SFO under $450), the search catalog only returns a single flight instead of all 3 matching departures.
- **Task**: Ensure `searchTrips()` scans the full catalog and returns every matching departure within the requested price range with available seats, without assuming catalog entries are pre-sorted by destination.

#### Issue 2: Connection Planner Chooses Sub-Optimal Layover Chains
- **User Symptom**: When a traveler requests the route with the fewest layovers between two cities (e.g., SEA to NYC), the engine returns a path with 3 flight legs (SEA → DEN → DFW → NYC) instead of the direct minimum 2-leg route (SEA → SFO → NYC).
- **Task**: Replace depth-first exploration in `fewestLayoverRoute()` with breadth-first search (BFS) to guarantee finding the path with the minimum number of flight legs.

#### Issue 3: Nested & Overlapping Bookings Shrink Available Windows
- **User Symptom**: When merging overlapping or nested busy intervals (e.g., a 00:10–00:40 block and a nested 00:15–00:20 block), the engine shrinks the busy interval's end time, exposing incorrect "free" windows.
- **Task**: In `availableWindows()`, ensure interval merging properly maintains the maximum end boundary (`std::max`) when processing overlapping and contained booked intervals.

#### Issue 4: Fare Finder Fails to Select Lowest Total Price Route
- **User Symptom**: When searching for the cheapest route between hubs (e.g., SEA to NYC), the engine chooses a 2-leg route costing $360 over an available 3-leg route that costs only $270.
- **Task**: Update `cheapestRoute()` to use a cost-weighted shortest-path algorithm (such as Dijkstra's algorithm) instead of unweighted FIFO queue traversal.

#### Issue 5: Group Pricing Calculates Total for Only One Passenger
- **User Symptom**: When creating a group reservation (e.g., 3 travelers on a $389 flight), the checkout total charges only $389 instead of multiplying by party size ($1,167).
- **Task**: Update `reservationTotalCents()` to calculate the reservation total across all travelers in the party.

#### Issue 6: Seat Inventory Over-Allocates Beyond Remaining Availability
- **User Symptom**: If a party requests more seats than remain available on a departure (e.g., 3 travelers when only 2 seats remain), the reservation still succeeds and drives inventory negative.
- **Task**: Update `reserveSeats()` to reject any reservation when the requested traveler count exceeds remaining seat inventory, leaving existing inventory unchanged.

---

## 3. Expected Behavior After Fixing Bugs

After resolving the issues:
1. Searching departures returns all 3 matching NYC → SFO options under $450.
2. Connection planning from SEA to NYC selects the fewest-layover route: `SEA → SFO → NYC`.
3. Flexible booking window calculation correctly outputs `00:00–00:10` and `00:40–01:00`.
4. The fare planner selects the lowest fare route of `$270` (`SEA → DEN → DFW → NYC`).
5. Group reservations accurately charge `$1,167` for a party of 3.
6. Attempting to book 3 travelers when only 2 seats remain is rejected and leaves 2 seats available.
7. All automated tests in `tests/` pass with exit code `0`.
