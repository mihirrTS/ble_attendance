ble_attendance — BLE-based attendance logger for ESP32

Overview
--------
This is a small ESP32 Arduino project that scans nearby Bluetooth Low Energy (BLE) advertisement packets and records "IN" and "OUT" attendance events for a small, pre-approved list of device names. The sketch (`ble_attendance.ino`) connects the ESP32 to Wi-Fi and sends attendance events to a Google Apps Script URL which can write to Google Sheets.

Key files
---------
- `ble_attendance.ino` — main Arduino sketch. Scans BLE, maintains in-memory presence map, and logs IN/OUT events via HTTP to a Google Apps Script endpoint.

How it works (quick)
--------------------
- On startup the ESP32 connects to Wi-Fi.
- BLE scan runs in repeated intervals (configured by `scanTime`).
- If a device from the whitelist (allowed devices) is detected and was previously absent, the sketch logs an "IN" event.
- If a device that was previously present is not seen in the current scan, it logs an "OUT" event.
- Events are sent using HTTP GET requests to the configured `GOOGLE_SCRIPT_URL`.

Configuration
-------------
Edit the top of `ble_attendance.ino` to set your Wi-Fi credentials and Google Apps Script URL. Update the `allowedDevices` array with the device names you want to track.

Limitations & assumptions
-------------------------
- The sketch uses device advertisement name string matching. Many BLE devices do not include a stable advertising name or may randomize addresses — this only works reliably for devices that advertise a consistent name.
- Presence is determined per-scan. The timeout and scan interval settings influence sensitivity to short absences.
- The Google Apps Script endpoint is assumed to accept unauthenticated GET requests. For production, replace with a secure backend.

Concise analysis — ways to handle, interpret, and record attendance data
------------------------------------------------------------------------
This section compares practical options you can use for storing and managing the whitelist and attendance logs. Each option has short pros/cons, complexity, cost, real-time capability, and recommended use-cases.

1) Google Sheets (Apps Script)
- Pros: Fast to set up, no server, familiar UI.
- Cons: Not real-time, rate limits, weak security, not for large scale.
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

