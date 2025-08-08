#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include <time.h>

// WiFi Credentials
const char* WIFI_SSID = "Getnet";
const char* WIFI_PASSWORD = "12345ghk";

// LED Pins
#define RED_LED_PIN 25
#define GREEN_LED_PIN 26
#define BLUE_LED_PIN 27

// System Settings
#define MAX_DEVICES 20
#define MAX_HISTORY 50
#define SCAN_INTERVAL 10000
#define EEPROM_SIZE 4096

AsyncWebServer server(80);

// Device Structure
typedef struct {
  String mac;
  String name;
  int rssi;
  bool authorized;
  time_t firstSeen;
  time_t lastSeen;
} NetworkDevice;

// Detection History
typedef struct {
  String mac;
  String name;
  time_t detectionTime;
  bool authorized;
} DetectionHistory;

NetworkDevice devices[MAX_DEVICES];
DetectionHistory history[MAX_HISTORY];
int deviceCount = 0;
int historyCount = 0;
unsigned long lastScanTime = 0;

// HTML Dashboard
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Network Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; padding: 20px; }
    table { width: 100%; border-collapse: collapse; margin-bottom: 20px; }
    th, td { padding: 8px; text-align: left; border-bottom: 1px solid #ddd; }
    tr:hover { background-color: #f5f5f5; }
    .auth { background-color: #e7f3fe; }
    .unauth { background-color: #ffebee; }
    .tabs { margin-bottom: 20px; }
    .tablinks { padding: 10px 15px; cursor: pointer; }
    .active { background-color: #ddd; }
    .tabcontent { display: none; }
    .button { 
      padding: 5px 10px; 
      background-color: #4CAF50;
      color: white;
      border: none;
      border-radius: 3px;
      cursor: pointer;
    }
  </style>
</head>
<body>
  <h1>Network Monitoring System</h1>
  
  <div class="tabs">
    <button class="tablinks active" onclick="openTab('devices')">Current Devices</button>
    <button class="tablinks" onclick="openTab('history')">Detection History</button>
  </div>

  <div id="devices" class="tabcontent" style="display:block;">
    <h2>Detected Devices</h2>
    <table id="deviceTable">
      <thead>
        <tr>
          <th>MAC Address</th>
          <th>Device Name</th>
          <th>Signal (RSSI)</th>
          <th>Status</th>
          <th>First Seen</th>
          <th>Last Seen</th>
          <th>Action</th>
        </tr>
      </thead>
      <tbody id="deviceList"></tbody>
    </table>
  </div>

  <div id="history" class="tabcontent">
    <h2>Detection History</h2>
    <table id="historyTable">
      <thead>
        <tr>
          <th>MAC Address</th>
          <th>Device Name</th>
          <th>Detection Time</th>
          <th>Status</th>
        </tr>
      </thead>
      <tbody id="historyList"></tbody>
    </table>
  </div>

  <script>
    function openTab(tabName) {
      const tabcontents = document.getElementsByClassName("tabcontent");
      for (let i = 0; i < tabcontents.length; i++) {
        tabcontents[i].style.display = "none";
      }
      
      const tablinks = document.getElementsByClassName("tablinks");
      for (let i = 0; i < tablinks.length; i++) {
        tablinks[i].className = tablinks[i].className.replace(" active", "");
      }
      
      document.getElementById(tabName).style.display = "block";
      event.currentTarget.className += " active";
    }

    function updateDeviceTable(devices) {
      let table = document.getElementById("deviceList");
      table.innerHTML = '';
      devices.forEach(device => {
        let row = table.insertRow();
        row.className = device.authorized ? "auth" : "unauth";
        row.insertCell(0).textContent = device.mac;
        row.insertCell(1).textContent = device.name || "Unknown";
        row.insertCell(2).textContent = device.rssi;
        row.insertCell(3).textContent = device.authorized ? "Authorized" : "Unauthorized";
        row.insertCell(4).textContent = new Date(device.firstSeen * 1000).toLocaleString();
        row.insertCell(5).textContent = new Date(device.lastSeen * 1000).toLocaleString();
        
        let actionCell = row.insertCell(6);
        if (!device.authorized) {
          let button = document.createElement("button");
          button.className = "button";
          button.textContent = "Authorize";
          button.onclick = function() { authorizeDevice(device.mac); };
          actionCell.appendChild(button);
        }
      });
    }

    function updateHistoryTable(history) {
      let table = document.getElementById("historyList");
      table.innerHTML = '';
      history.forEach(entry => {
        let row = table.insertRow();
        row.className = entry.authorized ? "auth" : "unauth";
        row.insertCell(0).textContent = entry.mac;
        row.insertCell(1).textContent = entry.name || "Unknown";
        row.insertCell(2).textContent = new Date(entry.detectionTime * 1000).toLocaleString();
        row.insertCell(3).textContent = entry.authorized ? "Authorized" : "Unauthorized";
      });
    }

    function authorizeDevice(mac) {
      fetch('/authorize', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'mac=' + encodeURIComponent(mac)
      }).then(response => {
        if (response.ok) {
          alert('Device authorized successfully!');
          fetchDevices();
        }
      });
    }

    function fetchDevices() {
      fetch('/devices')
        .then(response => response.json())
        .then(data => updateDeviceTable(data.devices));
    }

    function fetchHistory() {
      fetch('/history')
        .then(response => response.json())
        .then(data => updateHistoryTable(data.history));
    }

    setInterval(fetchDevices, 2000);
    fetchDevices();
    fetchHistory();
  </script>
</body>
</html>
)rawliteral";

// LED Functions
void setLED(const char* color) {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);
  if (strcmp(color, "red") == 0) digitalWrite(RED_LED_PIN, HIGH);
  else if (strcmp(color, "green") == 0) digitalWrite(GREEN_LED_PIN, HIGH);
  else if (strcmp(color, "blue") == 0) digitalWrite(BLUE_LED_PIN, HIGH);
}

void initLEDs() {
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  setLED("red");
}

// WiFi Connection
bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  
  Serial.println("Connecting to WiFi...");
  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
    setLED("green");
    return true;
  }
  
  Serial.println("\nConnection Failed!");
  setLED("red");
  return false;
}

// EEPROM Functions
void saveAuthorizedDevices() {
  DynamicJsonDocument doc(EEPROM_SIZE);
  JsonArray authDevices = doc.createNestedArray("devices");
  
  for (int i = 0; i < deviceCount; i++) {
    if (devices[i].authorized) {
      JsonObject device = authDevices.createNestedObject();
      device["mac"] = devices[i].mac;
      device["name"] = devices[i].name;
    }
  }
  
  String jsonStr;
  serializeJson(doc, jsonStr);
  
  EEPROM.begin(EEPROM_SIZE);
  for (size_t i = 0; i < jsonStr.length(); i++) {
    EEPROM.write(i, jsonStr[i]);
  }
  EEPROM.commit();
  EEPROM.end();
  
  Serial.println("Authorized devices saved to EEPROM");
}

void loadAuthorizedDevices() {
  EEPROM.begin(EEPROM_SIZE);
  String jsonStr;
  
  for (int i = 0; i < EEPROM_SIZE; i++) {
    byte val = EEPROM.read(i);
    if (val == 0) break;
    jsonStr += (char)val;
  }
  EEPROM.end();
  
  if (jsonStr.length() > 0) {
    DynamicJsonDocument doc(EEPROM_SIZE);
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (!error) {
      JsonArray authDevices = doc["devices"];
      for (JsonObject device : authDevices) {
        String mac = device["mac"].as<String>();
        
        // Check if device already exists
        bool exists = false;
        for (int i = 0; i < deviceCount; i++) {
          if (devices[i].mac == mac) {
            devices[i].authorized = true;
            exists = true;
            break;
          }
        }
        
        // Add new authorized device if not found
        if (!exists && deviceCount < MAX_DEVICES) {
          devices[deviceCount].mac = mac;
          devices[deviceCount].name = device["name"].as<String>();
          devices[deviceCount].authorized = true;
          devices[deviceCount].firstSeen = time(nullptr);
          devices[deviceCount].lastSeen = time(nullptr);
          deviceCount++;
        }
      }
      Serial.println("Authorized devices loaded from EEPROM");
    }
  }
}

// Network Scanning
void scanNetworks() {
  if (millis() - lastScanTime < SCAN_INTERVAL) return;
  
  Serial.println("\nScanning networks...");
  int found = WiFi.scanNetworks(false, true);
  Serial.printf("Found %d networks\n", found);
  
  for (int i = 0; i < found && deviceCount < MAX_DEVICES; i++) {
    String mac = WiFi.BSSIDstr(i);
    bool exists = false;
    
    // Check if device already exists
    for (int j = 0; j < deviceCount; j++) {
      if (devices[j].mac == mac) {
        devices[j].rssi = WiFi.RSSI(i);
        devices[j].lastSeen = time(nullptr);
        exists = true;
        
        // Add to history
        if (historyCount < MAX_HISTORY) {
          history[historyCount].mac = mac;
          history[historyCount].name = WiFi.SSID(i);
          history[historyCount].detectionTime = time(nullptr);
          history[historyCount].authorized = devices[j].authorized;
          historyCount++;
        }
        
        Serial.printf("Updated: %s (%ddBm)\n", mac.c_str(), WiFi.RSSI(i));
        break;
      }
    }
    
    // Add new device
    if (!exists) {
      devices[deviceCount].mac = mac;
      devices[deviceCount].name = WiFi.SSID(i);
      devices[deviceCount].rssi = WiFi.RSSI(i);
      devices[deviceCount].firstSeen = time(nullptr);
      devices[deviceCount].lastSeen = time(nullptr);
      devices[deviceCount].authorized = false;
      
      // Add to history
      if (historyCount < MAX_HISTORY) {
        history[historyCount].mac = mac;
        history[historyCount].name = WiFi.SSID(i);
        history[historyCount].detectionTime = time(nullptr);
        history[historyCount].authorized = false;
        historyCount++;
      }
      
      Serial.printf("Added: %s (%s) %ddBm\n", mac.c_str(), WiFi.SSID(i).c_str(), WiFi.RSSI(i));
      deviceCount++;
    }
  }
  
  WiFi.scanDelete();
  lastScanTime = millis();
}

// Web Server
void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/devices", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(2048);
    JsonArray jsonDevices = doc.createNestedArray("devices");
    
    for (int i = 0; i < deviceCount; i++) {
      JsonObject device = jsonDevices.createNestedObject();
      device["mac"] = devices[i].mac;
      device["name"] = devices[i].name;
      device["rssi"] = devices[i].rssi;
      device["authorized"] = devices[i].authorized;
      device["firstSeen"] = devices[i].firstSeen;
      device["lastSeen"] = devices[i].lastSeen;
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  server.on("/history", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(2048);
    JsonArray jsonHistory = doc.createNestedArray("history");
    
    for (int i = 0; i < historyCount; i++) {
      JsonObject entry = jsonHistory.createNestedObject();
      entry["mac"] = history[i].mac;
      entry["name"] = history[i].name;
      entry["detectionTime"] = history[i].detectionTime;
      entry["authorized"] = history[i].authorized;
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  server.on("/authorize", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("mac", true)) {
      String mac = request->getParam("mac", true)->value();
      
      for (int i = 0; i < deviceCount; i++) {
        if (devices[i].mac == mac) {
          devices[i].authorized = true;
          
          // Update history
          for (int j = 0; j < historyCount; j++) {
            if (history[j].mac == mac) {
              history[j].authorized = true;
            }
          }
          
          saveAuthorizedDevices();
          request->send(200, "text/plain", "Device authorized");
          return;
        }
      }
      request->send(404, "text/plain", "Device not found");
    } else {
      request->send(400, "text/plain", "MAC address required");
    }
  });

  server.begin();
  Serial.println("HTTP server started");
}

// Main Functions
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  initLEDs();
  EEPROM.begin(EEPROM_SIZE);
  loadAuthorizedDevices();
  
  if (connectWiFi()) {
    configTime(0, 0, "pool.ntp.org");
    Serial.println("Waiting for time sync...");
    while (time(nullptr) < 24 * 3600) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nTime synchronized!");
    
    setupWebServer();
    scanNetworks();
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    delay(5000);
  } else {
    scanNetworks();
    delay(1000);
  }
}