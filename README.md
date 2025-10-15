# BLE Attendance System — ESP32 Scanner + Mobile App

## How It Works Now (ESP32)

**What it does:** ESP32 scans for BLE devices, matches names against a whitelist, logs IN/OUT events to Google Sheets.

**Flow:**
1. ESP32 scans BLE advertisements every 5 seconds
2. Matches device names against hardcoded `allowedDevices[]` array
3. Logs "IN" when device first appears, "OUT" when no longer detected
4. Sends events via HTTP GET: `?device=MihirPhone&action=IN`
5. All state in RAM (lost on reboot)

**Key settings:**
- Scan: 5s active scan, 100ms interval, 99ms window
- Whitelist: `{"MihirPhone", "RaviWatch", "NehaLaptop"}`
- Logging: Unauthenticated HTTP GET to Google Apps Script
- File: `ble_attendance.ino`

**Current limitations:**
- Hardcoded whitelist (needs firmware update to change)
- No auth on logging endpoint (spammable)
- Name-based only (fails if phones randomize or don't advertise names)
- No retry/buffering (offline = lost events)
- No persistence across reboots


---

## What Your Mobile App Must Do

**Critical:** Phones must advertise BLE packets with "Complete Local Name" field matching the whitelist.

### Implementation by platform

**Android:** `BluetoothLeAdvertiser.startAdvertising()` with `setIncludeDeviceName(true)`
- Permissions: `BLUETOOTH_ADVERTISE` (Android 12+), `ACCESS_BACKGROUND_LOCATION`
- Background: May be throttled

**iOS:** `CBPeripheralManager.startAdvertising()` with `CBAdvertisementDataLocalNameKey`
- Permissions: `NSBluetoothAlwaysUsageDescription` in Info.plist
- Background: Enable "Uses Bluetooth LE accessories" mode; may be restricted

**React Native/Flutter:** Libraries like `react-native-ble-plx`, `flutter_blue_plus` have limited advertising support; may need native modules

### Testing workflow
1. Install **nRF Connect** app
2. Go to Advertiser tab → Create advertiser
3. Set "Complete Local Name" to whitelist name (e.g., "MihirPhone")
4. Enable "Connectable" and "Discoverable"
5. Start advertising → ESP logs "Walk-IN"
6. Stop → ESP logs "Walk-OUT" after next scan

### App user flow
1. Employee logs in → app fetches their BLE identifier from backend
2. App starts advertising with that name
3. Employee keeps app running (background OK if platform allows)
4. ESP detects and logs automatically
5. App shows today's IN/OUT times and history

**Important:**
- Battery impact: Continuous advertising drains battery; consider periodic mode
- Unique names: Each employee needs unique identifier (e.g., "MihirPhone", "RaviWatch")
- Fallback: Older devices may not support advertising → add QR/NFC/manual check-in


---

## Make Whitelist Dynamic (Options Ranked)

**Current:** Hardcoded `allowedDevices[]` in firmware

### 1. Google Sheets + Apps Script (Quickest)
Store names in Sheet, serve as JSON via Apps Script. ESP fetches on boot + every 15 min.
- **Pros:** 5 min setup, no server
- **Cons:** Not realtime, rate limits, weak auth
- **Use:** Prototypes, small teams

### 2. REST API + Database (Production)
Backend endpoint `/api/allowed-devices` returns JSON from Postgres/MySQL.
- **Pros:** Secure, scalable, instant updates
- **Cons:** Requires backend dev + hosting
- **Use:** Production, multiple ESPs

### 3. Firebase Realtime DB/Firestore
Store whitelist in Firebase, ESP uses Firebase ESP32 client.
- **Pros:** Realtime, easy app integration
- **Cons:** Firebase setup, library overhead
- **Use:** When using Firebase for everything

### 4. MQTT Topic
Publish whitelist to `/config/allowed-devices`, ESP subscribes.
- **Pros:** Realtime, scales to many ESPs
- **Cons:** Broker setup required
- **Use:** Large deployments (50+ ESPs)

**Recommended pattern (any option):**
1. Boot: Fetch remote list (5s timeout)
2. Success: Save to SPIFFS cache
3. Fail: Load SPIFFS cache → fallback to compiled array
4. Refresh every 15-30 min

**Security:** Use HTTPS + API key/token, validate JSON schema on ESP


---

## Better Data Storage (Options Ranked)

**Current:** Google Sheets (good for testing, not production)

| Option | Setup | Realtime | Scalability | Query Power | Cost (10k logs/mo) | Best For |
|--------|-------|----------|-------------|-------------|-------------------|----------|
| **REST + Postgres** | High | No* | High | Excellent | $5-20 | Production, custom needs |
| **Firebase** | Medium | Yes | High | Good | $0-25 | Startups, rapid dev |
| **Supabase** | Medium | Yes | High | Excellent | Free-$25 | Firebase + SQL power |
| **TimescaleDB** | High | No | Very High | Excellent | $10-30 | Analytics-heavy |
| **Google Sheets** | Very Low | No | Low | Poor | Free | Prototypes only |

*Add websockets for realtime

### Recommended: REST API + Postgres

**Backend endpoints:**
- `POST /api/attendance/log` — ESP logs events (returns 200 OK)
- `GET /api/allowed-devices` — ESP fetches whitelist
- `GET /api/attendance/history?employee_id=X&date=Y` — Employee app
- `GET /api/attendance/today` — Manager dashboard
- `POST /api/auth/login` — JWT token issuance

**Database schema:**
```sql
CREATE TABLE employees (
  id SERIAL PRIMARY KEY,
  name VARCHAR(100),
  ble_identifier VARCHAR(50) UNIQUE,
  email VARCHAR(100),
  role VARCHAR(50) -- 'employee' or 'manager'
);

CREATE TABLE attendance_logs (
  id SERIAL PRIMARY KEY,
  employee_id INT REFERENCES employees(id),
  action VARCHAR(10) CHECK (action IN ('IN', 'OUT')),
  timestamp TIMESTAMPTZ DEFAULT NOW(),
  esp_node_id VARCHAR(50)
);

CREATE INDEX idx_attendance_employee_date ON attendance_logs(employee_id, DATE(timestamp));
```

**ESP32 changes:**
- POST JSON: `{"device": "MihirPhone", "action": "IN", "esp_node_id": "ESP32_001"}`
- Add retry logic + SPIFFS buffering (max 100 events)
- Use API key in header: `Authorization: Bearer <key>`

**Tech stack suggestions:**
- Backend: Node.js/Express or Python/Flask
- Database: Postgres (or Supabase for hosted)
- Hosting: DigitalOcean ($5/mo), AWS, or Heroku
- Mobile: React Native or Flutter
- Web dashboard: React + Chart.js


---

## Dev Team Checklist

### Phase 1: Backend (Week 1-2)
- [ ] Choose storage (REST+Postgres or Firebase or Supabase)
- [ ] Implement API endpoints (whitelist, logging, auth)
- [ ] Add SECRET_KEY to current Google Script (quick security fix)
- [ ] Design DB schema (employees, attendance_logs)

### Phase 2: ESP32 Firmware (Week 2-3, parallel with Phase 1)
- [ ] Dynamic whitelist fetch (ArduinoJson library)
- [ ] SPIFFS caching + fallback logic
- [ ] Change logging from GET to POST JSON
- [ ] Retry logic + circular buffer (100 events)
- [ ] Add esp_node_id, firmware_version to logs
- [ ] OTA update support

### Phase 3: Mobile App (Week 3-5)
- [ ] Choose framework (React Native / Flutter / Native)
- [ ] Implement BLE advertising ("Complete Local Name")
- [ ] Test with nRF Connect → ESP32 (Android + iOS)
- [ ] Auth + fetch BLE identifier from backend
- [ ] Employee view: today's IN/OUT, history
- [ ] Background advertising + permissions
- [ ] Notifications ("Marked IN at 9:05 AM")

### Phase 4: Manager Dashboard (Week 5-6)
- [ ] Web app (React) or mobile manager view
- [ ] Team attendance (today, filters, export CSV)
- [ ] Analytics (late arrivals, hours worked)
- [ ] Whitelist management (add/remove employees)

### Phase 5: Testing & Deploy (Week 6-7)
- [ ] Load test (50+ phones, 1000 events/day)
- [ ] Battery test (24hr continuous advertising)
- [ ] Offline buffering test
- [ ] Multiple ESP nodes test
- [ ] Deploy backend + monitoring


---

## Quick Start (Current System)

**ESP32:**
1. Arduino IDE + ESP32 board support
2. Edit `ble_attendance.ino`: WiFi creds, Google Script URL, `allowedDevices[]`
3. Upload, open Serial Monitor (115200)

**Google Apps Script (current logging):**
```javascript
function doGet(e) {
  const sheet = SpreadsheetApp.getActiveSheet();
  sheet.appendRow([new Date(), e.parameter.device, e.parameter.action]);
  return ContentService.createTextOutput('OK');
}
```
Deploy as Web App → copy URL to sketch

**Test with nRF Connect:**
Advertiser → "Complete Local Name" → "MihirPhone" → Enable Connectable/Discoverable → Start

---

## Technical Notes

**BLE:** ESP scans advertisement packets (not connections). Uses "Complete Local Name" (0x09) field. Match is case-sensitive.

**Timing:** 5s scan + 3s pause = 8s cycle. Device must be absent for 1 full scan to trigger OUT.

**Power (ESP32):** Scanning 80-100mA, WiFi 100-150mA, idle 30-40mA. Use deep sleep if battery-powered.

**Scaling:** For large offices, use multiple ESP nodes with unique `esp_node_id`. Backend tracks which ESP saw which employee.

---

## Next Steps

**Recommended:** Choose Supabase or REST+Postgres → Implement endpoints → Update ESP firmware → Build mobile app

**Need help?** Contact maintainer for Firebase integration code, REST API examples, or mobile app starters.

- Complexity: Very low.
- Real-time: No (polling only).
- Best for: Small/home projects, prototypes.

2) Firebase (Realtime DB / Firestore)
- Pros: Realtime sync, secure rules, SDKs for web/mobile, scales.
- Cons: Requires setup; learning curve.
- Complexity: Medium.
- Real-time: Yes.
- Best for: Production systems or when mobile/web dashboards are required.

3) Self-hosted REST API + Database
- Pros: Full control, customizable business logic, works on LAN/cloud.
- Cons: Hosting and maintenance, higher complexity.
- Complexity: High.
- Real-time: Partial; implement server push or websockets for real-time.
- Best for: Custom enterprise features and integrations.

4) MQTT Broker (Mosquitto/CloudMQTT/EMQX)
- Pros: Lightweight, real-time, great for many devices.
- Cons: Broker setup and client logic for dashboards.
- Complexity: Medium.
- Real-time: Yes.
- Best for: Multiple ESP devices, low-latency systems.

5) Local JSON on ESP (SPIFFS / LittleFS / SD)
- Pros: Offline capable, simple.
- Cons: Manual updates, not multi-user, small storage.
- Complexity: Low.
- Real-time: No.
- Best for: Offline or single-device testing.

Decision guidance (short)
- Proof-of-concept / personal: Google Sheets or Local JSON.
- Production with app/dashboard: Firebase (or REST API if you want full control).
- Many ESP devices / real-time: MQTT (optionally combined with Firebase for dashboards).

Next steps I can do for you
--------------------------
- Add a simple Google Apps Script example to accept and write events to Google Sheets.
- Implement optional fetching of the whitelist from Google Sheets (so you can modify the allowed devices remotely).
- Port the project to use Firebase or MQTT with example code.

If you want one of those, tell me which and I’ll add it and update the sketch accordingly.

Developer handoff — current working state and prioritized future handling
---------------------------------------------------------------------

Purpose of this section: provide the dev team with everything needed to understand the current implementation, the data flow, known limitations, edge cases, and a prioritized, practical roadmap for future improvements.

Current working state (what's implemented now)
- Project: `ble_attendance.ino` — single Arduino sketch for ESP32.
- Network: The ESP32 connects to Wi-Fi using `WIFI_SSID` and `WIFI_PASS` defined at the top of the sketch.
- BLE scanning: Uses the ESP32 BLE library (`BLEDevice`, `BLEScan`) with an active scan. Scan duration = `scanTime` (seconds).
- Whitelist: An in-code array `allowedDevices[]` (device name strings) defines which devices to track.
- Presence tracking: In-memory map `devicePresence` (std::map<String,bool>) tracks whether each allowed device is currently "present".
- Logging: When a device transitions from false→true, an "IN" event is sent. When a device previously present is not seen in a scan, an "OUT" event is sent.
- Remote logging endpoint: `GOOGLE_SCRIPT_URL` — an HTTP GET request with `?device=&action=` is used to log events (intended for a Google Apps Script that writes to Google Sheets).
- Storage: No persistent storage on the ESP; all state is in RAM and lost on reboot.

High-level data flow
- ESP32 connects to Wi-Fi.
- BLE scan runs periodically and discovers advertised BLE devices.
- For each discovered device, if the device name matches an entry in `allowedDevices`, mark it seen and emit an "IN" event if previously absent.
- After each scan, compare the current seen set with `devicePresence`; for any device missing but previously present, emit an "OUT" event and update `devicePresence`.
- Send events to `GOOGLE_SCRIPT_URL` via HTTP GET.

Key files & symbols for the dev team
- `ble_attendance.ino` — main sketch.
  - `WIFI_SSID`, `WIFI_PASS`, `GOOGLE_SCRIPT_URL` — configuration constants.
  - `allowedDevices[]` — whitelist array.
  - `scanTime` — BLE scan duration in seconds.
  - `devicePresence` — in-memory presence map.
  - `logToGoogleSheet()` — HTTP logging helper.
  - `MyAdvertisedDeviceCallbacks::onResult()` — callback for asynchronous BLE results.

Assumptions and limitations (important for devs)
- BLE advertisement name matching: This only works when remote devices advertise a stable and known device name. Many phones/tablets and wearables randomize or omit names — may cause missed detections.
- No authentication for the Google Apps Script URL. If exposed publicly, malicious logging/spamming is possible.
- No persistence: reboots clear all presence. If continuity is required across reboots, a persistent store (SPIFFS/LittleFS/SD or external DB) is needed.
- Timing sensitivity: scanTime and the delay between scans determine sensitivity; short absences may be counted as OUT then IN repeatedly.
- Address randomization: Devices that randomize BLE MAC addresses will appear as new devices and won’t match by name reliably.

Data schema and example
- Attendance event (HTTP request currently sent as GET query parameters):
  - device: string (device name)
  - action: "IN" or "OUT"
  - timestamp: not explicitly sent (server should timestamp on receipt)

Example event URL
```
https://script.google.com/.../exec?device=MihirPhone&action=IN
```

Recommended prioritized roadmap (practicality + relevance)
1) Secure and reliable logging (High priority)
	- Replace the open Google Apps Script endpoint with a secured backend (API key, token-based, or restrict by IP). If using Google Apps Script, at minimum add a shared secret parameter and validate on the script side.
	- Server should record timestamps server-side to ensure consistent time.

2) Make whitelist remotely editable (High priority)
	- Implement remote whitelist fetching at boot and periodically (e.g., GET `/allowed-devices` returning JSON). Options:
	  - Quick: Google Sheets (Apps Script) returns a device list as JSON.
	  - Better: Firebase / REST API for secure and instantaneous updates.

3) Add persistence for presence & logs (Medium priority)
	- Use SPIFFS/LittleFS to persist the whitelist and last-known presence across reboots.
	- Optionally buffer logs locally when Wi-Fi is down and flush them when reconnected.

4) Improve detection reliability (Medium priority)
	- Use BLE address plus name (when available) and implement heuristics for address randomization (pairing/exchange tokens where possible).
	- Tune `scanTime`, `setInterval()`, and `setWindow()` for environment.

5) Migrate logging to a real backend with app/dashboard (Medium-High priority)
	- Firebase (quick to integrate, realtime dashboard) or
	- Self-hosted REST API with a database (Postgres/MySQL/Mongo) if you need full control and integrations.

6) Real-time updates & scaling (Lower priority / when needed)
	- Use MQTT for many ESP devices to publish attendance events to a broker and process them with a centralized consumer that writes to DB or Firebase.

7) Add authentication and device verification (Optional/High security)
	- Add device-side authentication tokens and validate events at the server.
	- Consider challenge-response or per-device shared secrets to reduce spoofing risk.

Edge cases + suggested handling
- Intermittent connectivity: Buffer outgoing events in a circular log and retry on reconnect.
- Rapid leave/return (bouncing): Implement debouncing — require N sequential scans (or a grace period) before marking OUT; or require M consecutive misses to declare OUT.
- Duplicate device names: If two different devices advertise the same name, either switch to address-based identification or require unique names per device.
- Device renaming or OS updates: Provide an admin mechanism to re-map names to identities.

Developer checklist for handoff
1. Review `ble_attendance.ino` and test in the deployment environment (Wi-Fi coverage & BLE device behaviors).
2. Add a secure logging endpoint or configure the existing Apps Script to validate a shared secret.
3. Implement remote whitelist fetching and decide where it will be hosted (Sheet / Firebase / REST API).
4. Add persistence and offline buffering if logs must survive reboots or outages.
5. Write integration tests (unit tests for parsing, functional test for end-to-end logging) and a small runner script to simulate BLE events (or a test mode in the sketch).
6. Document deployment steps: which pins (if any), power requirements, Wi-Fi config, and how to update `allowedDevices` safely.

Suggested immediate changes for safety and reliability (quick wins)
- Add a `SECRET_KEY` query parameter to `logToGoogleSheet()` and validate in the Apps Script to prevent spam.
- Add local buffering of failed HTTP requests and a small retry/backoff.
- Fetch the whitelist at boot from a remote source; fall back to the embedded `allowedDevices[]` if fetch fails.

Making the device list dynamic (prioritized methods)
-----------------------------------------------
You currently have `allowedDevices[]` hardcoded. Below are practical ways to make the device list dynamic, ordered from simplest to most robust. Each entry includes implementation notes, security, and caching/fallback recommendations.

1) Remote Google Sheet (quick, low effort)
	- How: Host the device list in a Google Sheet and expose it via a Google Apps Script endpoint that returns JSON, e.g. `{ "devices": ["MihirPhone","RaviWatch"] }`.
	- ESP: On boot (and periodically, e.g., every 5–15 minutes), GET the endpoint and parse JSON using `ArduinoJson`.
	- Security: Add a `SECRET_KEY` query param and validate it in the Apps Script. Consider rate limits and caching headers.
	- Fallback: Keep the compiled `allowedDevices[]` as fallback if fetch fails.

2) Simple REST API (recommended for teams)
	- How: Build a small REST endpoint `/allowed-devices` that returns a JSON array.
	- ESP: Use `HTTPClient` to fetch and parse the array. Optionally use an authorization header or API key.
	- Security: Use HTTPS, token-based auth (API key, JWT), and IP restrictions if feasible.
	- Fallback: Cache the last-successful response in SPIFFS/LittleFS and fall back to it on startup or when offline.

3) Firebase (Realtime updates)
	- How: Store `allowedDevices` in Realtime DB or Firestore. The ESP can use Firebase client libraries or poll for changes.
	- ESP: Firebase ESP32 client libraries exist but require configuration (API key, auth). Benefits: push-like behavior and SDK support.
	- Security: Use Firebase rules to allow read-only access to the whitelist for authorized devices.
	- Fallback: Optionally persist the last value locally.

4) MQTT (publish/update list)
	- How: Publish the current whitelist to a well-known topic (e.g., `/config/allowed-devices`) in JSON.
	- ESP: Subscribe to that topic and update local memory on message receive.
	- Security: Use broker auth, TLS. If offline, use retained messages so new clients get the last value.
	- Fallback: Keep local copy in SPIFFS as backup.

5) Manual (SD card / OTA configuration)
	- How: Drop a JSON file on an SD card, or push a new firmware/OTA that includes a new `allowedDevices` file.
	- Best when internet is unavailable. Fallback: compiled array as last resort.

JSON schema examples
- Simple array response (recommended):

```
{
  "devices": [
	 "MihirPhone",
	 "RaviWatch",
	 "NehaLaptop"
  ],
  "version": "2025-10-14T02:00:00Z"
}
```

- REST API response with metadata:

```
{
  "devices": ["MihirPhone","RaviWatch"],
  "updated_by": "admin@example.com",
  "updated_at": "2025-10-14T02:00:00Z"
}
```

Caching & fallback strategy (recommended pattern)
- On boot: try to fetch remote whitelist (timeout e.g., 5s).
- If fetch succeeds: validate and replace in-memory list; write to SPIFFS/LittleFS as cache.
- If fetch fails: read last cache from SPIFFS; if not present, fall back to compiled `allowedDevices[]` and log a warning.

Security considerations
- Always prefer HTTPS for remote endpoints.
- Use a shared secret or token to prevent unauthorized updates. For highest security, use server-side auth and per-device credentials.
- Validate JSON schema and string lengths on the device to avoid malformed input and memory issues.

What a custom-built app needs (requirements and suggested stack)
-----------------------------------------------------------
If you plan to replace or complement the nRF Connect testing with a custom mobile/web app, here's what the app should provide and what backend endpoints/features are recommended.

Minimum features for the custom app (MVP)
- Authentication: simple login for admins (to update whitelist, view logs).
- Whitelist management: view/add/remove device names or map names to friendly user IDs.
- Attendance dashboard: list today's IN/OUT events, filters by date/device, export CSV.
- Live view (optional): show devices currently present (requires real-time backend or frequent polling).
- Device health: show last-seen timestamp per ESP node, Wi-Fi/uptime status (if implemented in firmware).

Recommended backend endpoints
- GET /allowed-devices -> returns JSON device list (public key or read-only token allowed)
- POST /log-event -> accepts { device, action } and records event with server timestamp
- GET /logs?start=...&end=... -> paginated logs query for dashboard
- POST /auth/login -> issues tokens (if app has admin features)

Suggested tech stacks
- Mobile app: React Native or Flutter for cross-platform; or native Android/iOS if preferred.
- Web dashboard: React/Vue/Angular with a small Node/Express or Python Flask backend.
- Database: Postgres or Firebase depending on preference (Postgres for relational queries, Firebase for rapid realtime features).
- Hosting: Vercel/Netlify (frontend), Heroku/DigitalOcean/AWS (backend), or Firebase if you choose their stack.

Integration notes for the firmware
- Keep the logging API small and robust: accept minimal fields (device, action), validate on server, and add server timestamp.
- Consider adding a `node_id` to the ESP firmware so the backend can differentiate multiple ESP nodes.
- Add a `firmware_version` parameter to help troubleshoot device-side issues.

nRF Connect (testing) — how to set advertising packet for a test device
---------------------------------------------------------------
You're currently using nRF Connect to emulate advertiser packets (screenshot provided). The settings shown in the image correspond to creating a custom advertiser with a "Display name" and the advertisement field "Complete Local Name". Use these settings when you want the ESP scan to see a device by name.

Key settings to use in nRF Connect advertiser mode (matches your screenshot):
- Display name: set to the friendly name (e.g., "Mihir")
- Advertising data: include "Complete Local Name" with the exact string that appears in `allowedDevices[]` (e.g., "MihirPhone")
- Options: enable "Connectable" and "Discoverable" as shown — this results in standard advertising behavior the ESP32 scans will detect.

Notes about nRF Connect testing
- Make sure the advertised "Complete Local Name" exactly matches the whitelist entry in the firmware (case-sensitive match as implemented now).
- If you intend to test address-randomization behavior, toggle random address modes in the advertiser options and verify how the ESP behaves.
- Use the "Include Tx Power" option if you want to experiment with proximity heuristics based on RSSI and estimated transmit power.

Example test flow using nRF Connect
1. Create an advertiser with "Complete Local Name" set to a whitelist name.
2. Start advertising from nRF Connect on a phone; run the ESP sketch and check Serial for "Walk-IN" logs.
3. Stop advertising; once the ESP scan cycle completes and does not see the device, confirm the "Walk-OUT" log.

Developer handoff wrap-up
- README now contains: project summary, current working, prioritized improvements, dynamic whitelist methods, custom app requirements, and practical nRF Connect test guidance.
- If you want, I can now implement: secure logging (SECRET_KEY), remote whitelist fetch + cache, or local buffering of logs. Pick a priority and I will implement it and add tests/sample server code.

Contact & further support
- If you want, I can implement any of the above changes. Tell me which of the prioritized items you want first and I will update the sketch and provide example server/script code.

