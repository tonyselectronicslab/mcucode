#include "WiFi.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

const int BLE_SCAN_TIME_SECONDS = 5; 
BLEScan* pBLEScan;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        Serial.print("   [BLE] Device Found: ");
        Serial.print(advertisedDevice.getAddress().toString().c_str());
        if (advertisedDevice.haveName()) {
            Serial.printf(" | Name: %s", advertisedDevice.getName().c_str());
        }
        Serial.println();
    }
};

void setup() {
  // Opens a direct electrical line over the UART serial chip pins [1, 2]
  Serial.begin(115200);
  delay(1500);
  
  Serial.println("\n==============================================");
  Serial.println("DIYmore ESP32-S3 UART Environment Scanner Active");
  Serial.println("==============================================");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); 
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); 
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  
}

void loop() {
  Serial.println("\n[WI-FI] Starting Scan...");
  int wifiNetworksFound = WiFi.scanNetworks();
  
  if (wifiNetworksFound == 0) {
    Serial.println("   No Wi-Fi networks discovered.");
  } else {
    Serial.printf("   Success: %d Wi-Fi Networks Found:\n", wifiNetworksFound);
    for (int i = 0; i < wifiNetworksFound; ++i) {
      Serial.printf("   %2d: %-32s (%d dBm)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
  }
  WiFi.scanDelete(); 

  delay(1000);

  Serial.printf("\n[BLE] Starting %d-Second Bluetooth Scan...\n", BLE_SCAN_TIME_SECONDS);
  BLEScanResults* foundDevices = pBLEScan->start(BLE_SCAN_TIME_SECONDS, false);
  Serial.printf("   Scan Finished. Total BLE devices tracked: %d\n", foundDevices->getCount());
  pBLEScan->clearResults(); 

  Serial.println("\n----------------------------------------------");
  delay(5000);
}

