# BLE Attendance System — ESP32 Scanner + App Backend

## Current Working (ESP32 Side)

### What the ESP32 does
The `ble_attendance.ino` sketch implements a BLE proximity-based attendance logger:

1. **BLE Scanning**: ESP32 continuously scans for nearby Bluetooth Low Energy (BLE) advertisement packets every `scanTime` seconds (default: 5 seconds).

2. **Name Matching**: For each detected BLE device, the ESP extracts the advertised device name and checks if it matches any entry in the hardcoded `allowedDevices[]` whitelist array.

3. **Walk-IN Detection**: When a whitelisted device appears for the first time (not previously marked as present), the ESP logs a "Walk-IN" event.

4. **Walk-OUT Detection**: After each scan cycle, if a device that was previously present is no longer detected, the ESP logs a "Walk-OUT" event.

5. **Remote Logging**: Events are sent via HTTP GET request to a Google Apps Script URL (`GOOGLE_SCRIPT_URL`) in the format:
   ```
   ?device=MihirPhone&action=IN
   ```

6. **In-Memory State**: The ESP maintains a presence map (`devicePresence`) in RAM. All state is lost on reboot.

### Technical details (ESP32)
- **BLE Library**: Uses ESP32's native `BLEDevice`, `BLEScan`, and `BLEAdvertisedDevice` APIs
- **Scan Configuration**: 
  - Active scan mode (requests scan response packets)
  - Scan interval: 100ms, scan window: 99ms
  - Scan duration: configurable via `scanTime` (default 5 seconds)
- **Whitelist**: Hardcoded string array `allowedDevices[]` = `{"MihirPhone", "RaviWatch", "NehaLaptop"}`
- **Logging**: HTTP GET to Google Apps Script (unauthenticated, no retry logic)
- **No Persistence**: Presence state and logs are not stored locally; reboots reset everything

### Key files
- `ble_attendance.ino` — complete Arduino sketch for ESP32

### Current limitations
- **Hardcoded whitelist**: Device names are compiled into firmware
- **No authentication**: Google Apps Script URL is open; anyone with the URL can spam logs
- **Name-based only**: Relies on BLE devices advertising a stable "Complete Local Name" — many modern phones randomize MAC addresses and may not advertise names consistently
- **No offline buffering**: Failed HTTP requests are dropped
- **Reboot loses state**: All presence information is lost on power cycle

---

## What the Mobile App Must Do (For ESP to Detect Phones)

For employee phones to be detected by the ESP32 scanner, the mobile app must configure BLE advertising correctly. Here's what app developers need to implement:

### Required: BLE Advertising with "Complete Local Name"

The ESP32 scans for BLE advertisement packets and matches against the **Complete Local Name** field. The mobile app must:

1. **Enable BLE Advertising** (not just scanning)
   - Use platform-specific APIs:
     - **Android**: `BluetoothLeAdvertiser` class
     - **iOS**: `CBPeripheralManager` with advertising enabled
   - **React Native**: Use libraries like `react-native-ble-manager` or `react-native-ble-plx` (advertising support varies)
   - **Flutter**: Use `flutter_blue_plus` or `flutter_reactive_ble` with advertiser mode

2. **Set the "Complete Local Name" in advertisement packet**
   - This field must contain the exact string that matches the whitelist on the ESP (e.g., `"MihirPhone"`)
   - The name should be unique per employee
   - **Important**: The match is case-sensitive as currently implemented

3. **Keep advertising active**
   - Advertising should run continuously (or at least periodically) while the app is in foreground/background
   - Use platform-specific background modes and permissions to keep advertising alive when app is backgrounded

### Platform-specific implementation notes

#### Android
- Use `BluetoothLeAdvertiser.startAdvertising()` with `AdvertiseData` containing:
  - `setIncludeDeviceName(true)` OR manually add name via `addServiceData()` or manufacturer data
  - Alternatively, use `AdvertiseData.Builder().setIncludeDeviceName(true)` and ensure the Bluetooth adapter name is set to the employee's identifier
- **Permissions required**: `BLUETOOTH_ADVERTISE` (Android 12+), `BLUETOOTH`, `BLUETOOTH_ADMIN` (older)
- **Background considerations**: Requires `ACCESS_BACKGROUND_LOCATION` on some Android versions; advertising may be throttled

#### iOS
- Use `CBPeripheralManager` and call `startAdvertising()` with `CBAdvertisementDataLocalNameKey` set to the employee's identifier
- **Permissions required**: `NSBluetoothAlwaysUsageDescription` in Info.plist
- **Background considerations**: Enable "Uses Bluetooth LE accessories" background mode; iOS may restrict advertising in background

#### Cross-platform (React Native / Flutter)
- Advertising support is limited in many BLE libraries; verify library capabilities before choosing
- May require native bridge code for full advertising control
- Consider using native modules if library doesn't expose advertising APIs

### Testing with nRF Connect (for developers)

Before building the app, test BLE detection using the **nRF Connect** app:

1. Open nRF Connect and go to the **Advertiser** tab
2. Create a new advertiser profile:
   - **Display name**: Any friendly name (e.g., "Mihir")
   - **Advertising data**: Add record → Select **"Complete Local Name"** → Enter the exact whitelist name (e.g., "MihirPhone")
   - **Options**: Enable **"Connectable"** and **"Discoverable"**
3. Start advertising
4. Run the ESP32 sketch and check Serial Monitor for "Walk-IN detected: MihirPhone"
5. Stop advertising and verify "Walk-OUT detected: MihirPhone" appears after the next scan cycle

### App workflow for employees
1. Employee opens the app and logs in (authenticate user)
2. App fetches the employee's assigned BLE identifier from backend (e.g., "MihirPhone")
3. App starts BLE advertising with that identifier as the "Complete Local Name"
4. Employee keeps the app running (foreground or background) while at work
5. ESP detects the phone and logs IN/OUT events automatically
6. App can display today's attendance status, history, and notifications

### Important considerations for app developers
- **Battery impact**: Continuous BLE advertising drains battery; implement power-saving modes or periodic advertising
- **Name collisions**: Ensure each employee has a unique BLE identifier; use a naming convention like `"FirstnamePhone"` or assign UUIDs
- **Platform restrictions**: iOS and Android have different background advertising capabilities; test on real devices
- **Fallback for non-advertising devices**: Some devices may not support advertising (especially older iOS versions); consider alternative check-in methods (QR code, NFC, manual)

---

## Making the Whitelist Dynamic (Replace Hardcoded Names)

Currently, `allowedDevices[]` is hardcoded in the firmware. Here are practical methods to make it dynamic, ordered by implementation complexity:

### Option 1: Fetch from Google Sheets via Apps Script (Quickest)

**How it works:**
- Store employee BLE identifiers in a Google Sheet (one column: `DeviceName`)
- Create a Google Apps Script that reads the sheet and returns JSON:
  ```javascript
  function doGet(e) {
    const sheet = SpreadsheetApp.openById('SHEET_ID').getSheetByName('Devices');
    const data = sheet.getRange('A2:A').getValues().flat().filter(String);
    return ContentService.createTextOutput(JSON.stringify({ devices: data }))
      .setMimeType(ContentService.MimeType.JSON);
  }
  ```
- ESP32 fetches this endpoint at boot (and optionally every 15 minutes)

**ESP32 implementation:**
- Add `ArduinoJson` library
- HTTP GET request to Apps Script URL
- Parse JSON and populate `allowedDevices[]` dynamically
- Keep hardcoded array as fallback if fetch fails

**Pros:** Very fast to set up, no server needed, familiar Google Sheets interface  
**Cons:** Not real-time, rate limits, no auth by default  
**Best for:** Small teams, proof-of-concept

### Option 2: REST API + Database (Recommended for Production)

**How it works:**
- Build a backend API with endpoint: `GET /api/allowed-devices`
- Returns JSON: `{ "devices": ["MihirPhone", "RaviWatch", ...] }`
- Backend reads from a database (Postgres, MySQL, MongoDB, Firebase)

**ESP32 implementation:**
- HTTP GET with optional API key in header: `Authorization: Bearer <token>`
- Parse JSON response
- Cache last successful response in SPIFFS/LittleFS for offline fallback

**Pros:** Secure, scalable, real-time updates, integrates with employee management system  
**Cons:** Requires backend development and hosting  
**Best for:** Production systems with multiple ESPs and employees

**Database schema example:**
```sql
CREATE TABLE employees (
  id SERIAL PRIMARY KEY,
  name VARCHAR(100),
  ble_identifier VARCHAR(50) UNIQUE,
  is_active BOOLEAN DEFAULT TRUE
);
```

API returns only `ble_identifier` where `is_active = TRUE`.

### Option 3: Firebase Realtime Database / Firestore (Easy + Realtime)

**How it works:**
- Store whitelist in Firebase:
  ```json
  {
    "allowedDevices": {
      "MihirPhone": true,
      "RaviWatch": true
    }
  }
  ```
- ESP32 uses Firebase ESP32 client library to read and listen for changes

**Pros:** Real-time updates, easy mobile SDK integration, Firebase handles auth/rules  
**Cons:** Requires Firebase setup, library overhead on ESP32  
**Best for:** When using Firebase for the entire backend (app + attendance logs)

### Option 4: MQTT Topic (For Multiple ESPs)

**How it works:**
- Publish whitelist updates to MQTT topic `/config/allowed-devices`
- ESP subscribes to this topic and updates whitelist on message receive
- Use retained messages so new ESPs get the last value on connect

**Pros:** Real-time, lightweight, scales to many ESP devices  
**Cons:** Requires MQTT broker setup  
**Best for:** Large deployments with many ESP nodes

### Recommended caching & fallback pattern (for any option)

1. **At boot**: Attempt to fetch remote whitelist (5-10 second timeout)
2. **On success**: Parse JSON, validate, update in-memory array, save to SPIFFS as cache
3. **On failure**: Load from SPIFFS cache; if not present, use compiled fallback array
4. **Periodic refresh**: Re-fetch every 15-30 minutes (configurable)

**Security:**
- Use HTTPS for all HTTP requests
- Add authentication (API key, JWT token, or shared secret)
- Validate JSON structure and string lengths on ESP to prevent buffer overflows

---

## Better Data Storage (Replace Google Sheets)

Google Sheets works for prototyping but has limitations for production. Here are better alternatives for storing attendance logs that integrate with employee and manager apps:

### Option 1: REST API + Relational Database (PostgreSQL / MySQL)

**Architecture:**
- Backend API with endpoints:
  - `POST /api/attendance/log` — ESP submits IN/OUT events
  - `GET /api/attendance/history?employee_id=X&date=Y` — App queries logs
  - `GET /api/attendance/today` — Manager dashboard
- Database stores: employees, attendance_logs, esp_nodes

**Database schema:**
```sql
CREATE TABLE employees (
  id SERIAL PRIMARY KEY,
  name VARCHAR(100),
  ble_identifier VARCHAR(50) UNIQUE,
  email VARCHAR(100),
  role VARCHAR(50)
);

CREATE TABLE attendance_logs (
  id SERIAL PRIMARY KEY,
  employee_id INT REFERENCES employees(id),
  action VARCHAR(10), -- 'IN' or 'OUT'
  timestamp TIMESTAMPTZ DEFAULT NOW(),
  esp_node_id VARCHAR(50),
  CONSTRAINT valid_action CHECK (action IN ('IN', 'OUT'))
);

CREATE INDEX idx_attendance_employee_date ON attendance_logs(employee_id, DATE(timestamp));
```

**ESP32 changes:**
- Change `logToGoogleSheet()` to POST JSON:
  ```json
  {
    "device": "MihirPhone",
    "action": "IN",
    "esp_node_id": "ESP32_001"
  }
  ```
- Add retry logic and local buffering in SPIFFS if POST fails

**App integration:**
- **Employee view**: Query `/api/attendance/history?employee_id=123` to show personal attendance
- **Manager view**: Query `/api/attendance/today` or `/api/reports/monthly` for team overview
- Export to CSV/PDF for reports

**Pros:** Full control, complex queries, relational integrity, scales well  
**Cons:** Requires backend development, hosting, and maintenance  
**Best for:** Production systems, enterprise

### Option 2: Firebase (Firestore or Realtime Database)

**Architecture:**
- ESP logs events to Firebase
- Mobile app reads from same Firebase database
- Use Firebase security rules to control access (employees see only their data, managers see all)

**Firestore schema:**
```javascript
/attendance/{logId}: {
  employee_id: "MihirPhone",
  action: "IN",
  timestamp: Timestamp,
  esp_node_id: "ESP32_001"
}

/employees/{employeeId}: {
  name: "Mihir",
  ble_identifier: "MihirPhone",
  role: "employee"
}
```

**ESP32 changes:**
- Use Firebase ESP32 Client library
- Push attendance events to `/attendance` collection

**App integration:**
- **Employee view**: Query Firestore where `employee_id == currentUser.ble_identifier`
- **Manager view**: Query all documents with date filters
- Real-time updates: use Firestore listeners for live dashboard

**Pros:** Real-time sync, easy mobile integration, no backend code needed, Firebase Auth  
**Cons:** Learning curve, vendor lock-in, costs scale with usage  
**Best for:** Startups, rapid prototyping, real-time dashboards

### Option 3: Self-hosted TimescaleDB (PostgreSQL extension for time-series)

**Why TimescaleDB:**
- Optimized for time-series data (attendance logs are time-series by nature)
- Automatic partitioning and fast queries for date ranges
- Drop-in PostgreSQL replacement

**Schema:**
```sql
CREATE TABLE attendance_logs (
  time TIMESTAMPTZ NOT NULL,
  employee_id INT,
  action VARCHAR(10),
  esp_node_id VARCHAR(50)
);

SELECT create_hypertable('attendance_logs', 'time');
```

**Pros:** Excellent for analytics, fast queries, open-source  
**Cons:** Requires PostgreSQL knowledge and hosting  
**Best for:** Large scale, analytics-heavy systems

### Option 4: Supabase (Firebase alternative, open-source)

**What is Supabase:**
- Open-source Firebase alternative built on PostgreSQL
- Provides REST API, real-time subscriptions, and authentication
- Self-hostable or use their cloud hosting

**ESP32 changes:**
- POST to Supabase REST API: `/rest/v1/attendance_logs`
- Use API key or JWT for auth

**App integration:**
- Use Supabase client SDKs (JavaScript, Dart/Flutter, Swift)
- Real-time subscriptions for live updates
- Built-in auth and row-level security

**Pros:** Open-source, PostgreSQL power, real-time, generous free tier  
**Cons:** Smaller ecosystem than Firebase  
**Best for:** Teams wanting Firebase-like features with SQL database

### Comparison: Data Storage Options

| Feature | Google Sheets | REST + Postgres | Firebase | Supabase |
|---------|---------------|-----------------|----------|----------|
| Setup Complexity | Very Low | High | Medium | Medium |
| Real-time Updates | No | No (unless websockets) | Yes | Yes |
| Scalability | Low | High | High | High |
| Query Flexibility | Low | High | Medium | High |
| Cost (10k logs/month) | Free | ~$5-20 | ~$0-25 | Free-$25 |
| Employee App Integration | Hard | Easy | Easy | Easy |
| Manager Dashboard | Manual | Custom | Custom | Custom |
| Analytics/Reports | Limited | Excellent | Good | Excellent |

### Recommended Architecture (Production-ready)

**Backend:**
- REST API (Node.js/Express or Python/Flask)
- PostgreSQL database (or Supabase)
- Hosted on DigitalOcean, AWS, or Heroku

**ESP32:**
- Fetches whitelist from `/api/allowed-devices` every 15 min
- POSTs attendance events to `/api/attendance/log` with retry + buffering
- Stores failed requests in SPIFFS circular buffer (max 100 events)

**Mobile App (Employee):**
- React Native or Flutter
- Login with email/password (JWT auth)
- Fetch assigned BLE identifier from `/api/employees/me`
- Start BLE advertising with that identifier
- View personal attendance: GET `/api/attendance/history?employee_id=me`
- Show today's IN/OUT times, weekly summary

**Web Dashboard (Manager):**
- React + Chart.js or similar
- View all employees' attendance today: GET `/api/attendance/today`
- Filters by date range, employee, export CSV
- Analytics: late arrivals, early departures, total hours

**Security:**
- HTTPS everywhere
- JWT tokens for app authentication
- ESP uses API key (stored in firmware, rotatable via OTA)
- Rate limiting on all endpoints
- Database backup and logging

---

## Developer Handoff Checklist

### Immediate tasks (before app development)
1. ✅ Review `ble_attendance.ino` and test current Google Sheets logging
2. ⬜ Add `SECRET_KEY` parameter to Google Apps Script logging (quick security win)
3. ⬜ Decide on data storage backend (Firebase vs REST API vs Supabase)
4. ⬜ Design database schema for employees and attendance_logs
5. ⬜ Implement backend API endpoints (whitelist + logging)

### ESP32 firmware updates (in parallel with backend)
1. ⬜ Add dynamic whitelist fetching (HTTP GET + JSON parsing)
2. ⬜ Implement SPIFFS caching and fallback logic
3. ⬜ Change logging to POST JSON instead of GET query params
4. ⬜ Add retry logic and local buffering for failed requests
5. ⬜ Add `esp_node_id` and `firmware_version` to logs
6. ⬜ Implement OTA updates for remote firmware management

### Mobile app development
1. ⬜ Choose framework (React Native vs Flutter vs Native)
2. ⬜ Implement BLE advertising with "Complete Local Name"
3. ⬜ Test advertising on Android and iOS with nRF Connect → ESP32
4. ⬜ Implement authentication (login, JWT storage)
5. ⬜ Fetch employee's BLE identifier from backend
6. ⬜ Build employee attendance view (today's IN/OUT, history)
7. ⬜ Handle background advertising and permissions
8. ⬜ Add notifications (e.g., "You've been marked IN")

### Manager dashboard (web or mobile)
1. ⬜ Build authentication (separate manager role)
2. ⬜ Implement team attendance view (all employees, today)
3. ⬜ Add filters: date range, employee name, action (IN/OUT)
4. ⬜ Export attendance reports (CSV, PDF)
5. ⬜ Analytics: late arrivals, total hours, monthly summaries
6. ⬜ Manage whitelist (add/remove employees, assign BLE identifiers)

### Testing & deployment
1. ⬜ Test ESP32 with multiple phones advertising simultaneously
2. ⬜ Test walk-in/walk-out detection with different scan intervals
3. ⬜ Test offline buffering and retry logic
4. ⬜ Load test backend API (simulate 50+ employees, 1000 events/day)
5. ⬜ Test app battery consumption with continuous advertising
6. ⬜ Deploy backend to production (with monitoring and backups)
7. ⬜ Document deployment (Wi-Fi config, API keys, OTA process)

---

## Quick Start (Current System)

### ESP32 Setup
1. Install Arduino IDE and ESP32 board support
2. Install libraries: `WiFi`, `HTTPClient`, `BLEDevice` (built-in on ESP32)
3. Open `ble_attendance.ino`
4. Edit:
   - `WIFI_SSID` and `WIFI_PASS`
   - `GOOGLE_SCRIPT_URL` (your Apps Script web app URL)
   - `allowedDevices[]` array
5. Upload to ESP32
6. Open Serial Monitor (115200 baud) to see logs

### Google Apps Script Setup (Current)
1. Create a Google Sheet with columns: `Timestamp`, `Device`, `Action`
2. Go to Extensions → Apps Script
3. Paste:
   ```javascript
   function doGet(e) {
     const sheet = SpreadsheetApp.getActiveSheet();
     const device = e.parameter.device;
     const action = e.parameter.action;
     sheet.appendRow([new Date(), device, action]);
     return ContentService.createTextOutput('OK');
   }
   ```
4. Deploy as Web App (Anyone can access)
5. Copy the web app URL to `GOOGLE_SCRIPT_URL` in the sketch

### Testing with nRF Connect
1. Install nRF Connect app on phone
2. Go to Advertiser tab → Create advertiser
3. Set "Complete Local Name" to a name from `allowedDevices[]` (e.g., "MihirPhone")
4. Enable "Connectable" and "Discoverable"
5. Start advertising
6. Check ESP32 Serial Monitor for "Walk-IN detected"
7. Stop advertising and verify "Walk-OUT detected"

---

## Technical Notes

### BLE Advertisement Packet Structure
- The ESP scans for BLE advertisement packets (not connections)
- Relevant field: **Complete Local Name** (0x09) or **Shortened Local Name** (0x08)
- Current code uses `advertisedDevice.getName()` which extracts this field
- Some devices don't advertise names by default; app must explicitly include it

### Timing & Sensitivity
- `scanTime = 5` seconds: how long each scan runs
- `delay(3000)` between scans: 3-second pause
- Total cycle: ~8 seconds
- A device must be absent for one full scan to trigger "Walk-OUT"
- Adjust these values for environment (larger offices may need longer scans)

### Power Consumption (ESP32)
- Active BLE scanning: ~80-100mA
- Wi-Fi active: ~100-150mA
- Idle (between scans): ~30-40mA
- Use deep sleep between scans to reduce power if battery-powered

### Scaling (Multiple ESPs)
- For large offices, deploy multiple ESP32 nodes
- Each node should have a unique `esp_node_id`
- Backend can track which ESP detected the employee (useful for zone-based attendance)

---

## Support & Contact

For implementation questions or to request specific features (Firebase integration, REST API example, mobile app starter code), contact the project maintainer or open an issue in the repository.

**Next recommended step**: Choose a backend (Supabase or REST API), implement the endpoints, then update the ESP firmware to use dynamic whitelist + improved logging.

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

