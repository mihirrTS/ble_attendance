#include <WiFi.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <map>

// =================== WIFI CONFIG ===================
const char* WIFI_SSID = "Zoo_Studio_2.4";
const char* WIFI_PASS = "Trh@1234";
const char* GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbyzkxCLyA7MfOrPnXirxHPIT7XOWx8LQJG_Bade2zxOSSZJ--0eePnHCIMRhP2zbz_V/exec";

// =================== BLE CONFIG ===================
int scanTime = 5; // seconds
BLEScan* pBLEScan;

// Whitelist of BLE Device Names (employees)
const char* allowedDevices[] = {"MihirPhone", "RaviWatch", "NehaLaptop"};
int allowedCount = sizeof(allowedDevices) / sizeof(allowedDevices[0]);

// Track device states (present/absent)
std::map<String, bool> devicePresence;  

// =================== HTTP Logging ===================
void logToGoogleSheet(String deviceName, String action) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(GOOGLE_SCRIPT_URL) + "?device=" + deviceName + "&action=" + action;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.printf("Logged %s (%s): %d\n", deviceName.c_str(), action.c_str(), httpCode);
    } else {
      Serial.printf("Error logging %s (%s)\n", deviceName.c_str(), action.c_str());
    }
    http.end();
  } else {
    Serial.println("WiFi disconnected. Cannot log.");
  }
}

// =================== BLE Callback ===================
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String devName = advertisedDevice.getName().c_str();
    if (devName.length() == 0) return; // skip unnamed devices

    for (int i = 0; i < allowedCount; i++) {
      if (devName.equals(allowedDevices[i])) {
        if (!devicePresence[devName]) { // First time seen
          Serial.printf("Walk-IN detected: %s [%s]\n", devName.c_str(), advertisedDevice.getAddress().toString().c_str());
          logToGoogleSheet(devName, "IN");
          devicePresence[devName] = true;
        }
      }
    }
  }
};

// =================== SETUP ===================
void setup() {
  Serial.begin(115200);
  delay(2000);

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

  // Initialize map
  for (int i = 0; i < allowedCount; i++) {
    devicePresence[String(allowedDevices[i])] = false;
  }
}

// =================== LOOP ===================
void loop() {
  Serial.println("Starting BLE scan...");
  BLEScanResults* foundDevices = pBLEScan->start(scanTime, false);

  // Reset "seen" status each scan
  std::map<String, bool> currentSeen;
  for (int i = 0; i < allowedCount; i++) {
    currentSeen[allowedDevices[i]] = false;
  }

  // Mark devices seen in this scan
  for (int j = 0; j < foundDevices->getCount(); j++) {
    BLEAdvertisedDevice d = foundDevices->getDevice(j);
    String devName = String(d.getName().c_str());
    if (devName.length() == 0) continue;

    for (int i = 0; i < allowedCount; i++) {
      if (devName == allowedDevices[i]) {
        currentSeen[devName] = true;

        if (!devicePresence[devName]) { // First time seen = Walk-IN
          Serial.printf("Walk-IN detected: %s [%s]\n", devName.c_str(), d.getAddress().toString().c_str());
          logToGoogleSheet(devName, "IN");
          devicePresence[devName] = true;
        }
      }
    }
  }

  // Check for walk-outs (devices previously present but not seen now)
  for (int i = 0; i < allowedCount; i++) {
    String dev = allowedDevices[i];
    if (!currentSeen[dev] && devicePresence[dev]) {
      Serial.printf("Walk-OUT detected: %s\n", dev.c_str());
      logToGoogleSheet(dev, "OUT");
      devicePresence[dev] = false;
    }
  }

  Serial.println("Scan done!");
  pBLEScan->clearResults();
  delay(3000);
}

