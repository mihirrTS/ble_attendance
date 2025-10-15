# BT Classic + BLE Hybrid Attendance System

## What's Different Here

This hybrid approach combines **Bluetooth Classic pairing** with **BLE scanning** to create a permission-based attendance system. Unlike the main `ble_attendance.ino` which matches device names, this version:

1. **Requires explicit pairing** between ESP32 and employee phones
2. **Tracks devices by MAC address** (not advertised name)
3. **Stores paired device list** in ESP32 flash memory (persists across reboots)
4. **Only logs paired devices** - unknown BLE devices are ignored

---

## Setup Instructions

### 1. Initial Configuration

Same WiFi and Google Sheets setup as main system (see main README.md).

### 2. Hardware Setup

- ESP32 board with Bluetooth Classic + BLE support
- BOOT button (GPIO 0) used to enter pairing mode
- No additional hardware required

### 3. First Time Pairing

**On ESP32:**
1. Upload `hybrid_attendance.ino` to ESP32
2. Open Serial Monitor (115200 baud)
3. **Press and hold BOOT button for 2 seconds**
4. ESP32 enters pairing mode and broadcasts "ESP32-Attendance"

**On Employee Phone:**
1. Open phone Bluetooth settings
2. Pair with "ESP32-Attendance" (may require PIN: 1234 or 0000)
3. Install a Bluetooth Serial Terminal app:
   - Android: "Serial Bluetooth Terminal" by Kai Morich
   - iOS: "Bluetooth Terminal" by Micro Ware Lab
4. Connect to "ESP32-Attendance" in the terminal app
5. ESP32 will prompt: `Enter device MAC address (format: AA:BB:CC:DD:EE:FF):`
6. Enter employee name (e.g., "John Smith")
7. **Find phone's Bluetooth MAC address:**
   - Android: Settings → About Phone → Status → Bluetooth address
   - iOS: Settings → General → About → Bluetooth (only visible when connected)
8. Enter MAC address in format `AA:BB:CC:DD:EE:FF`
9. ESP32 confirms: `Device paired: John Smith (AA:BB:CC:DD:EE:FF)`
10. Press BOOT button again to exit pairing mode

**Repeat for each employee** (max 20 devices).

### 4. Daily Operation

- ESP32 automatically scans for BLE advertisements
- Only paired device MACs trigger walk-in/out logging
- No user interaction needed after pairing
- Paired devices persist in flash memory (no re-pairing after reboot)

---

## Limitations

### 1. **MAC Address Randomization (CRITICAL)**

**Problem:** Modern phones randomize BLE MAC addresses for privacy.

- **iOS:** Randomizes MAC every 15 minutes by default
- **Android 10+:** Randomizes MAC per-network or periodically

**Impact:** Even after pairing, the BLE advertisement MAC may differ from the paired MAC, breaking detection.

**Workarounds:**
- Android: Some devices allow disabling randomization in Developer Options
- iOS: No user-facing disable option
- Alternative: Use encrypted BLE payload instead (see main README.md alternatives)

### 2. **Manual MAC Entry**

ESP32 BT Classic API doesn't easily expose connected device MAC during pairing, so this implementation requires **manual MAC address input** via serial terminal. This adds friction to onboarding.

### 3. **Onboarding Friction**

Each employee must:
- Pair via Bluetooth settings
- Install Bluetooth Serial Terminal app
- Manually enter name and MAC address
- Understand their own device's MAC address

This is **not practical** for large deployments (50+ employees).

### 4. **Device Limit**

ESP32 flash storage limits paired devices to ~20-30 maximum. For larger teams, would need external storage (SD card, cloud sync).

### 5. **Single ESP32 per Location**

Each employee must pair individually with every ESP32 unit. Multi-location setups require re-pairing at each site.

### 6. **No Real Bluetooth Bonding**

This implementation uses BT Classic for the pairing *workflow* (name/MAC exchange) but doesn't leverage actual BT bonding security. It's essentially a UI for building a MAC whitelist.

---

## Pros

✅ **Explicit Consent:** Employees must actively pair, creating clear permission trail  
✅ **MAC-Based Identity:** More unique than advertised names (which can be spoofed)  
✅ **Persistent Storage:** Paired devices survive reboots  
✅ **Prevents Name Spoofing:** Random person can't just rename their device to "MihirPhone"  

---

## Cons

❌ **Doesn't Solve MAC Randomization:** Modern phones will still change BLE MAC  
❌ **High Onboarding Friction:** Manual pairing + MAC entry for each employee  
❌ **Poor Scalability:** Max 20 devices, single ESP32, no cloud sync  
❌ **Maintenance Overhead:** Lost/replaced phones require manual re-pairing  
❌ **Limited Security:** BT Classic is vulnerable to sniffing during pairing  

---

## Comparison to Main Approach

| Feature | Main (Name-Based) | Hybrid (Pairing) |
|---------|-------------------|------------------|
| **Setup Complexity** | Simple (hardcode names) | Complex (per-device pairing) |
| **Onboarding** | Just advertise with name | Pair + MAC entry |
| **Scalability** | Unlimited names | Max 20-30 devices |
| **MAC Randomization** | Not affected (uses name) | **Breaks detection** |
| **Spoofing Resistance** | Low (anyone can use name) | Medium (needs MAC) |
| **Multi-Location** | Same names work everywhere | Re-pair each ESP32 |
| **Cloud Sync** | Easy (just update name list) | Requires custom sync logic |
| **Recommended For** | **Production use** | Proof-of-concept only |

---

## Recommendation

**For production, use the main name-based approach** (`ble_attendance.ino`) with one of these security layers:

1. **AES Encrypted Payload:** App encrypts employee ID in BLE advertisement (see main README.md)
2. **Custom BLE Service:** App advertises Service UUID + Characteristic with encrypted employee ID
3. **Backend Validation:** App registers with backend, ESP32 queries whitelist API in real-time

The hybrid pairing approach is **not recommended** due to MAC randomization rendering it ineffective on modern phones.

---

## Code Architecture

### Key Components

**`hybrid_attendance.ino`:**
- `loadPairedDevices()`: Reads MAC+name pairs from ESP32 flash (Preferences library)
- `savePairedDevice()`: Stores new pairing to flash
- `enterPairingMode()`: BT Classic serial pairing workflow
- `isDevicePaired()`: Checks if scanned BLE MAC is in paired list
- `MyAdvertisedDeviceCallbacks::onResult()`: Only processes paired MACs

**Storage:**
- Uses ESP32 `Preferences` library (NVS flash)
- Namespace: `bt_paired`
- Keys: `count`, `mac_0`, `name_0`, `mac_1`, `name_1`, etc.

**Pairing Flow:**
1. Long-press BOOT → `SerialBT.begin("ESP32-Attendance")`
2. User pairs via phone Bluetooth settings
3. User connects via Bluetooth Serial Terminal
4. ESP32 prompts for name → user sends "John Smith"
5. ESP32 prompts for MAC → user sends "AA:BB:CC:DD:EE:FF"
6. ESP32 saves to flash + adds to `pairedDevices` vector
7. Short-press BOOT → exit pairing, resume BLE scanning

### Memory Usage

- Each paired device: ~40 bytes (17-byte MAC + 20-byte name + overhead)
- 20 devices ≈ 800 bytes flash
- Flash limit: 512KB NVS partition (plenty of space)
- **Actual limit:** Practical UX (managing 20+ manual pairings)

---

## Testing

1. Upload `hybrid_attendance.ino`
2. Long-press BOOT button on ESP32
3. Pair from phone Bluetooth settings
4. Use Serial Bluetooth Terminal to enter name + MAC
5. Check Serial Monitor for confirmation
6. Walk away → check Serial Monitor for "Walk-OUT"
7. Return → check Serial Monitor for "Walk-IN"
8. Verify Google Sheets logs show MAC address

**To clear all pairings:**
Uncomment `clearAllPairedDevices()` in `setup()`, re-upload, then comment it out again.

---

## When to Use This

✅ Small team (<10 people) with technical users  
✅ Controlled environment (can disable MAC randomization)  
✅ Proof-of-concept or security research  
✅ Legacy Android devices (pre-randomization)  

❌ Production deployment with modern phones  
❌ Large teams (onboarding friction)  
❌ iOS devices (cannot disable randomization)  
❌ Multi-location setups  

---

## See Also

- **main README.md:** General BLE concepts, data storage options, app requirements
- **ble_attendance.ino:** Production-ready name-based implementation
- **Encrypted Payload Alternative:** See "Better Security" section in main README.md
