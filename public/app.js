const $ = (selector) => document.querySelector(selector);
const state = { trips: [], saved_itineraries: [], reservations: [], fewest_layovers: [], cheapest_route: {}, open_windows: [] };
let shownTrips = [];
let selectedTrip = null;
let sortByPrice = false;

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (character) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[character]);
}
function money(cents) { return `$${(Number(cents || 0) / 100).toLocaleString("en-US", { minimumFractionDigits: 0, maximumFractionDigits: 0 })}`; }
function duration(minutes) { return `${Math.floor(minutes / 60)}h ${minutes % 60}m`; }
function prettyTime(value) {
  const [hour, minute] = value.split(":").map(Number);
  const suffix = hour >= 12 ? "PM" : "AM";
  return `${hour % 12 || 12}:${String(minute).padStart(2, "0")} ${suffix}`;
}
function clock(minutes) {
  const hour = Math.floor(minutes / 60);
  return `${hour % 12 || 12}:${String(minutes % 60).padStart(2, "0")} ${hour >= 12 ? "PM" : "AM"}`;
}

async function loadState() {
  try {
    const response = await fetch("api/state", { headers: { Accept: "application/json" } });
    if (!response.ok) throw new Error("We couldn't refresh your trip board.");
    Object.assign(state, await response.json());
    renderInsights();
    renderSaved();
    renderReservations();
    await searchFlights(false);
  } catch (error) {
    $("#flight-list").innerHTML = `<div class="empty-state">${escapeHtml(error.message)} Please try again shortly.</div>`;
  }
}

function renderInsights() {
  $("#fewest-route").textContent = state.fewest_layovers.join(" → ") || "No route found";
  $("#cheap-route").textContent = (state.cheapest_route.airports || []).join(" → ") || "No route found";
  $("#cheap-price").textContent = `${Math.max(1, (state.cheapest_route.airports || []).length - 1)} flights · from ${money(state.cheapest_route.total_price_cents)}`;
  const windows = state.open_windows || [];
  $("#window-list").innerHTML = windows.length ? windows.slice(0, 3).map((window, index) => `<div class="window-row"><span class="window-dot dot-${index + 1}"></span><span>${clock(window.start)} <i>—</i> ${clock(window.end)}</span><small>${Math.floor((window.end - window.start) / 60)}h ${String((window.end - window.start) % 60).padStart(2, "0")}m open</small></div>`).join("") : `<div class="empty-state compact-empty">No open windows for this day.</div>`;
}

function renderFlightList() {
  const trips = sortByPrice ? [...shownTrips].sort((a, b) => a.price_cents - b.price_cents) : shownTrips;
  $("#results-caption").textContent = `${trips.length} options for ${$("#origin").value.toUpperCase()} → ${$("#destination").value.toUpperCase()} · Wed, Oct 14`;
  $("#flight-list").innerHTML = trips.length ? trips.map((trip) => `<article class="flight-card">
    <div class="airline-mark ${trip.airline.includes("Pacific") ? "pacific" : trip.airline.includes("Coast") ? "coast" : "northstar-mark"}">${trip.airline.includes("Pacific") ? "p" : trip.airline.includes("Coast") ? "c" : "✳"}</div>
    <div class="flight-main"><div class="flight-times"><strong>${prettyTime(trip.departure)}</strong><span class="flight-track"><i></i></span><strong>${prettyTime(trip.arrival)}</strong></div><div class="flight-details"><span>${escapeHtml(trip.airline)}</span><i>·</i><span>${escapeHtml(trip.origin)} → ${escapeHtml(trip.destination)}</span><i>·</i><span>${trip.stops === 0 ? "Nonstop" : `${trip.stops} stop`}</span><i>·</i><span>${duration(trip.duration_minutes)}</span></div></div>
    <div class="flight-price"><span>from</span><strong>${money(trip.price_cents)}</strong><small>${trip.seats_available} seats left</small></div>
    <div class="flight-actions"><button class="save-flight" data-save="${escapeHtml(trip.id)}" aria-label="Save this flight">♡</button><button class="choose-flight" data-book="${escapeHtml(trip.id)}">Choose <span>→</span></button></div>
  </article>`).join("") : `<div class="empty-state"><span>✳</span><strong>No departures found just yet.</strong><small>Try another city or widen your travel dates.</small></div>`;
  document.querySelectorAll("[data-book]").forEach((button) => button.addEventListener("click", () => openBooking(button.dataset.book)));
  document.querySelectorAll("[data-save]").forEach((button) => button.addEventListener("click", () => saveTrip(button.dataset.save)));
}

async function searchFlights(showToast = false) {
  const origin = $("#origin").value.trim().toUpperCase();
  const destination = $("#destination").value.trim().toUpperCase();
  if (!origin || !destination) {
    $("#flight-list").innerHTML = `<div class="empty-state">Choose both a starting point and a destination.</div>`;
    return;
  }
  try {
    const params = new URLSearchParams({ origin, destination, maxPrice: "1000000" });
    const response = await fetch(`api/search?${params.toString()}`, { headers: { Accept: "application/json" } });
    if (!response.ok) throw new Error("Flight search is unavailable right now.");
    shownTrips = (await response.json()).trips || [];
    renderFlightList();
    if (showToast) toast(`Updated departures for ${origin} → ${destination}`);
  } catch (error) {
    $("#flight-list").innerHTML = `<div class="empty-state">${escapeHtml(error.message)}</div>`;
  }
}

function renderSaved() {
  const saved = state.saved_itineraries || [];
  $("#saved-count").textContent = saved.length;
  $("#saved-list").innerHTML = saved.slice(0, 3).map((item, index) => {
    const trip = state.trips.find((option) => option.id === item.trip_id);
    const destination = trip ? trip.destination_name : "Your next destination";
    const artClass = ["saved-art coast-art", "saved-art city-art", "saved-art desert-art"][index % 3];
    return `<article class="saved-row"><div class="${artClass}"><span>${index === 0 ? "✦" : index === 1 ? "⌁" : "✳"}</span></div><div><strong>${escapeHtml(item.title)}</strong><small>${escapeHtml(destination)} · ${trip ? trip.date : "Flexible dates"}</small></div><button class="saved-arrow" data-saved-trip="${escapeHtml(item.trip_id)}" aria-label="Explore saved trip">↗</button></article>`;
  }).join("") || `<div class="empty-state compact-empty">Save a departure to start an itinerary.</div>`;
  document.querySelectorAll("[data-saved-trip]").forEach((button) => button.addEventListener("click", () => {
    const trip = state.trips.find((option) => option.id === button.dataset.savedTrip);
    if (trip) { $("#origin").value = trip.origin; $("#destination").value = trip.destination; searchFlights(true); window.scrollTo({ top: 0, behavior: "smooth" }); }
  }));
}

function renderReservations() {
  const reservations = state.reservations || [];
  $("#reservation-list").innerHTML = reservations.length ? reservations.slice().reverse().slice(0, 3).map((reservation) => {
    const trip = state.trips.find((option) => option.id === reservation.trip_id);
    return `<article class="reservation-row"><span class="reservation-check ${reservation.status === "cancelled" ? "cancelled" : ""}">${reservation.status === "cancelled" ? "×" : "✓"}</span><div><strong>${escapeHtml(trip ? `${trip.origin} → ${trip.destination}` : reservation.trip_id)}</strong><small>${escapeHtml(reservation.id)} · ${reservation.travelers} traveler${reservation.travelers === 1 ? "" : "s"} · ${escapeHtml(reservation.status)}</small></div><b>${money(reservation.total_cents)}</b>${reservation.status === "confirmed" ? `<button class="cancel-reservation" data-cancel="${escapeHtml(reservation.id)}">Cancel</button>` : ""}</article>`;
  }).join("") : `<div class="empty-state compact-empty">Confirmed trips will show up here.</div>`;
  document.querySelectorAll("[data-cancel]").forEach((button) => button.addEventListener("click", () => cancelReservation(button.dataset.cancel)));
}

async function cancelReservation(reservationId) {
  if (!window.confirm(`Cancel reservation ${reservationId}?`)) return;
  try {
    const response = await fetch(`api/reservations/${encodeURIComponent(reservationId)}`, { method: "PATCH", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ status: "cancelled" }) });
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || "Could not update this reservation.");
    await loadState();
    toast(`${reservationId} cancelled. Seats returned to the departure.`);
  } catch (error) { toast(error.message); }
}

function openBooking(tripId) {
  selectedTrip = state.trips.find((trip) => trip.id === tripId);
  if (!selectedTrip) return;
  const travelers = Number($("#travelers").value);
  $("#selected-flight").innerHTML = `<div class="selected-route"><span>${escapeHtml(selectedTrip.origin)}</span><i>→</i><span>${escapeHtml(selectedTrip.destination)}</span></div><div class="selected-meta">${escapeHtml(selectedTrip.airline)} · ${prettyTime(selectedTrip.departure)} · ${duration(selectedTrip.duration_minutes)}</div>`;
  $("#traveler-summary").textContent = `${travelers} traveler${travelers === 1 ? "" : "s"}`;
  $("#booking-total").textContent = money(selectedTrip.price_cents * travelers);
  $("#booking-message").textContent = "";
  $("#booking-modal").hidden = false;
  $("#traveler-name").focus();
}

function closeBooking() { $("#booking-modal").hidden = true; }

async function confirmBooking() {
  if (!selectedTrip) return;
  const button = $("#confirm-booking");
  button.disabled = true;
  const payload = { trip_id: selectedTrip.id, travelers: Number($("#travelers").value), traveler_name: $("#traveler-name").value.trim() || "Traveler" };
  try {
    const response = await fetch("api/reservations", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(payload) });
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || "We couldn't confirm this reservation.");
    await loadState();
    $("#booking-message").textContent = `${result.id} confirmed. Your trip is on its way.`;
    window.setTimeout(closeBooking, 1100);
  } catch (error) { $("#booking-message").textContent = error.message; }
  finally { button.disabled = false; }
}

async function saveTrip(tripId) {
  const trip = state.trips.find((option) => option.id === tripId);
  if (!trip) return;
  try {
    const response = await fetch("api/itineraries", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ title: `${trip.destination_name} getaway`, trip_id: trip.id }) });
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || "Could not save this itinerary.");
    await loadState();
    toast(`${result.title} saved to your itineraries`);
  } catch (error) { toast(error.message); }
}

let toastTimer;
function toast(message) {
  const node = $("#toast");
  node.textContent = message;
  node.classList.add("visible");
  window.clearTimeout(toastTimer);
  toastTimer = window.setTimeout(() => node.classList.remove("visible"), 2600);
}

$("#search-form").addEventListener("submit", (event) => { event.preventDefault(); sortByPrice = false; searchFlights(true); });
$("#swap-cities").addEventListener("click", () => {
  const origin = $("#origin").value;
  $("#origin").value = $("#destination").value;
  $("#destination").value = origin;
});
document.querySelectorAll(".destination-chip").forEach((button) => button.addEventListener("click", () => {
  $("#destination").value = button.dataset.destination;
  sortByPrice = false;
  searchFlights(true);
}));
$("#sort-price").addEventListener("click", () => {
  sortByPrice = !sortByPrice;
  $("#sort-price").innerHTML = `${sortByPrice ? "Lowest price" : "Best match"} <span>⌄</span>`;
  renderFlightList();
});
$("#load-more").addEventListener("click", () => toast("You're seeing every available departure for these filters."));
$("#side-search").addEventListener("click", () => { $("#destination").value = "SFO"; searchFlights(true); window.scrollTo({ top: 0, behavior: "smooth" }); });
$("#travelers").addEventListener("change", () => {
  if (selectedTrip && !$("#booking-modal").hidden) openBooking(selectedTrip.id);
});
$("#confirm-booking").addEventListener("click", confirmBooking);
$("#close-modal").addEventListener("click", closeBooking);
$("#booking-modal").addEventListener("click", (event) => { if (event.target.id === "booking-modal") closeBooking(); });
$("#calendar-info").addEventListener("click", () => toast("Windows are calculated from existing saved departures."));
document.addEventListener("keydown", (event) => { if (event.key === "Escape") closeBooking(); });
loadState();
window.setInterval(loadState, 5000);
