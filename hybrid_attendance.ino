#include <WiFi.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BluetoothSerial.h>
#include <Preferences.h>
#include <map>

// =================== WIFI CONFIG ===================
const char* WIFI_SSID = "Zoo_Studio_2.4";
const char* WIFI_PASS = "Trh@1234";
const char* GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbyzkxCLyA7MfOrPnXirxHPIT7XOWx8LQJG_Bade2zxOSSZJ--0eePnHCIMRhP2zbz_V/exec";

// =================== BLE CONFIG ===================
int scanTime = 5; // seconds
BLEScan* pBLEScan;

// Track device states (present/absent) by MAC address
std::map<String, bool> devicePresence;
std::map<String, String> deviceNames; // MAC -> friendly name mapping

// =================== BLUETOOTH CLASSIC ===================
BluetoothSerial SerialBT;
Preferences preferences;

const int MAX_PAIRED_DEVICES = 20;
const char* PREF_NAMESPACE = "bt_paired";
const char* PREF_COUNT_KEY = "count";

// Button for pairing mode (GPIO 0 - BOOT button on most ESP32 boards)
const int PAIR_BUTTON_PIN = 0;
bool pairingMode = false;

// =================== PAIRING MANAGEMENT ===================
struct PairedDevice {
  String mac;
  String name;
};

std::vector<PairedDevice> pairedDevices;

void loadPairedDevices() {
  preferences.begin(PREF_NAMESPACE, false);
  int count = preferences.getInt(PREF_COUNT_KEY, 0);
  
  Serial.printf("Loading %d paired devices from storage...\n", count);
  
  for (int i = 0; i < count; i++) {
    String macKey = "mac_" + String(i);
    String nameKey = "name_" + String(i);
    
    String mac = preferences.getString(macKey.c_str(), "");
    String name = preferences.getString(nameKey.c_str(), "Unknown");
    
    if (mac.length() > 0) {
      PairedDevice dev = {mac, name};
      pairedDevices.push_back(dev);
      devicePresence[mac] = false;
      deviceNames[mac] = name;
      Serial.printf("  [%d] %s (%s)\n", i, name.c_str(), mac.c_str());
    }
  }
  
  preferences.end();
}

void savePairedDevice(String mac, String name) {
  if (pairedDevices.size() >= MAX_PAIRED_DEVICES) {
    Serial.println("ERROR: Max paired devices reached!");
    return;
  }
  
  // Check if already exists
  for (const auto& dev : pairedDevices) {
    if (dev.mac == mac) {
      Serial.println("Device already paired!");
      return;
    }
  }
  
  preferences.begin(PREF_NAMESPACE, false);
  
  int count = preferences.getInt(PREF_COUNT_KEY, 0);
  String macKey = "mac_" + String(count);
  String nameKey = "name_" + String(count);
  
  preferences.putString(macKey.c_str(), mac);
  preferences.putString(nameKey.c_str(), name);
  preferences.putInt(PREF_COUNT_KEY, count + 1);
  
  preferences.end();
  
  PairedDevice dev = {mac, name};
  pairedDevices.push_back(dev);
  devicePresence[mac] = false;
  deviceNames[mac] = name;
  
  Serial.printf("Saved: %s (%s)\n", name.c_str(), mac.c_str());
}

void clearAllPairedDevices() {
  preferences.begin(PREF_NAMESPACE, false);
  preferences.clear();
  preferences.end();
  
  pairedDevices.clear();
  devicePresence.clear();
  deviceNames.clear();
  
  Serial.println("All paired devices cleared!");
}

bool isDevicePaired(String mac) {
  for (const auto& dev : pairedDevices) {
    if (dev.mac.equalsIgnoreCase(mac)) {
      return true;
    }
  }
  return false;
}

// =================== HTTP Logging ===================
void logToGoogleSheet(String mac, String action) {
  String name = deviceNames[mac];
  
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(GOOGLE_SCRIPT_URL) + "?device=" + name + "&mac=" + mac + "&action=" + action;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.printf("Logged %s (%s) [%s]: %d\n", name.c_str(), mac.c_str(), action.c_str(), httpCode);
    } else {
      Serial.printf("Error logging %s (%s)\n", name.c_str(), action.c_str());
    }
    http.end();
  } else {
    Serial.println("WiFi disconnected. Cannot log.");
  }
}

// =================== BLE Callback ===================
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String mac = advertisedDevice.getAddress().toString().c_str();
    
    // Only process if device is paired
    if (isDevicePaired(mac)) {
      if (!devicePresence[mac]) { // First time seen
        Serial.printf("Walk-IN detected: %s [%s]\n", deviceNames[mac].c_str(), mac.c_str());
        logToGoogleSheet(mac, "IN");
        devicePresence[mac] = true;
      }
    }
  }
};

// =================== PAIRING MODE ===================
void enterPairingMode() {
  Serial.println("\n========================================");
  Serial.println("PAIRING MODE ACTIVATED");
  Serial.println("========================================");
  Serial.println("Instructions:");
  Serial.println("1. Open Bluetooth settings on your phone");
  Serial.println("2. Pair with 'ESP32-Attendance'");
  Serial.println("3. After pairing, send employee name via");
  Serial.println("   Bluetooth Serial (use Serial Bluetooth");
  Serial.println("   Terminal app)");
  Serial.println("4. Press BOOT button again to exit");
  Serial.println("========================================\n");
  
  SerialBT.begin("ESP32-Attendance");
  pairingMode = true;
  
  while (pairingMode) {
    // Check for button press to exit
    if (digitalRead(PAIR_BUTTON_PIN) == LOW) {
      delay(50); // debounce
      if (digitalRead(PAIR_BUTTON_PIN) == LOW) {
        while (digitalRead(PAIR_BUTTON_PIN) == LOW); // wait for release
        pairingMode = false;
        Serial.println("\nExiting pairing mode...");
        break;
      }
    }
    
    // Check for incoming Bluetooth Serial data
    if (SerialBT.available()) {
      String employeeName = SerialBT.readStringUntil('\n');
      employeeName.trim();
      
      if (employeeName.length() > 0) {
        // Get connected device MAC
        // Note: ESP32 BT Classic doesn't easily expose connected device MAC
        // This is a LIMITATION - we use a workaround with manual MAC entry
        SerialBT.println("Enter device MAC address (format: AA:BB:CC:DD:EE:FF):");
        
        // Wait for MAC address
        while (!SerialBT.available()) {
          delay(100);
        }
        
        String macAddress = SerialBT.readStringUntil('\n');
        macAddress.trim();
        macAddress.toUpperCase();
        
        if (macAddress.length() == 17) { // AA:BB:CC:DD:EE:FF
          savePairedDevice(macAddress, employeeName);
          SerialBT.printf("Device paired: %s (%s)\n", employeeName.c_str(), macAddress.c_str());
          Serial.printf("Device paired: %s (%s)\n", employeeName.c_str(), macAddress.c_str());
        } else {
          SerialBT.println("Invalid MAC address format!");
        }
      }
    }
    
    delay(100);
  }
  
  SerialBT.end();
  Serial.println("Pairing mode exited. Resuming BLE scanning...\n");
}

// =================== SETUP ===================
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  pinMode(PAIR_BUTTON_PIN, INPUT_PULLUP);

  Serial.println("BLE + BT Classic Hybrid Attendance System");
  Serial.println("==========================================");
  
  // Load paired devices from flash
  loadPairedDevices();
  
  Serial.printf("\nTotal paired devices: %d\n", pairedDevices.size());
  Serial.println("Hold BOOT button for 2 seconds to enter pairing mode\n");

  // WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // BLE
  Serial.println("Initializing BLE...");
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  Serial.println("System ready!\n");
}

// =================== LOOP ===================
void loop() {
  // Check for long press on BOOT button to enter pairing mode
  static unsigned long buttonPressStart = 0;
  static bool buttonWasPressed = false;
  
  if (digitalRead(PAIR_BUTTON_PIN) == LOW) {
    if (!buttonWasPressed) {
      buttonPressStart = millis();
      buttonWasPressed = true;
    } else if (millis() - buttonPressStart > 2000) {
      // Long press detected
      enterPairingMode();
      buttonWasPressed = false;
    }
  } else {
    buttonWasPressed = false;
  }
  
  // Normal BLE scanning
  Serial.println("Starting BLE scan...");
  BLEScanResults* foundDevices = pBLEScan->start(scanTime, false);

  // Reset "seen" status each scan
  std::map<String, bool> currentSeen;
  for (const auto& dev : pairedDevices) {
    currentSeen[dev.mac] = false;
  }

  // Mark devices seen in this scan
  for (int j = 0; j < foundDevices->getCount(); j++) {
    BLEAdvertisedDevice d = foundDevices->getDevice(j);
    String mac = d.getAddress().toString().c_str();
    
    if (isDevicePaired(mac)) {
      currentSeen[mac] = true;

      if (!devicePresence[mac]) { // First time seen = Walk-IN
        Serial.printf("Walk-IN detected: %s [%s]\n", deviceNames[mac].c_str(), mac.c_str());
        logToGoogleSheet(mac, "IN");
        devicePresence[mac] = true;
      }
    }
  }

  // Check for walk-outs (devices previously present but not seen now)
  for (const auto& dev : pairedDevices) {
    if (!currentSeen[dev.mac] && devicePresence[dev.mac]) {
      Serial.printf("Walk-OUT detected: %s [%s]\n", dev.name.c_str(), dev.mac.c_str());
      logToGoogleSheet(dev.mac, "OUT");
      devicePresence[dev.mac] = false;
    }
  }

  Serial.println("Scan done!");
  pBLEScan->clearResults();
  delay(3000);
}
