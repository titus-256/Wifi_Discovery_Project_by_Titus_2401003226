Network Monitoring System - ESP32 Based Device Tracker

Table of Contents
🧨Overview

🧨Features

🧨Hardware Requirements

🧨Software Requirements

🧨Setup Instructions

🧨How It Works

🧨Web Interface

🧨Data Storage



🧧Overview
This project is an ESP32-based network monitoring system that scans for nearby WiFi devices, tracks their presence, and provides a web interface to view and manage detected devices. The system can distinguish between authorized and unauthorized devices, maintaining a history of all detections.

Features
Scans for nearby WiFi devices at regular intervals

Tracks MAC addresses, device names, and signal strength (RSSI)

Maintains detection history with timestamps

Web-based dashboard for monitoring

Ability to authorize/unauthorize devices

Persistent storage of authorized devices in EEPROM

Visual status indication using RGB LED

Time synchronization via NTP

🧧🧧 Hardware Requirements
ESP32 development board

RGB LED (common cathode)

Red pin → GPIO 25

Green pin → GPIO 26

Blue pin → GPIO 27

Breadboard and jumper wires

USB cable for power and programming

🧧🧧 Software Requirements
Arduino IDE with ESP32 support

Required libraries:

WiFi.h

AsyncTCP.h

ESPAsyncWebServer.h

EEPROM.h

ArduinoJson.h

esp_wifi.h

time.h

🧧🧧 Setup Instructions
Clone this repository or download the source code

Open the project in Arduino IDE

Install all required libraries

Modify the WiFi credentials in the code:

cpp
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";
Connect the hardware as specified

Upload the code to your ESP32

Open the Serial Monitor (115200 baud) to monitor the system

🧧🧧 How It Works
Device Scanning
The ESP32 periodically scans for nearby WiFi networks (every 10 seconds by default)

For each detected network, it captures:

MAC address (BSSID)

Network name (SSID)

Signal strength (RSSI)

Timestamp of detection

🧧🧧 Device Tracking
New devices are added to the tracking list

Known devices have their information updated (last seen time, RSSI)

Each detection is recorded in the history log

Authorization System
Unknown devices are marked as unauthorized by default

Authorized devices are stored in EEPROM for persistence

The web interface allows authorizing new devices

🧧🧧 Web Interface
Provides a real-time dashboard showing:

Currently detected devices

Detection history

Allows management of device authorization status

Web Interface
The system provides a responsive web interface with two tabs:

🧧🧧 Current Devices Tab
Shows all currently detected devices

Displays MAC address, device name, signal strength, and timestamps

Color-coded by authorization status

Includes buttons to authorize unauthorized devices

🧧🧧 Detection History Tab
Shows a log of all detections

Includes timestamps and authorization status

Helps track when devices were first and last seen

🧧🧧 Data Storage
Authorized Devices: Stored in ESP32's EEPROM for persistence across reboots

Detection History: Maintained in memory (volatile, cleared on reboot)

Current Devices: Maintained in memory (volatile, repopulated during scans)

🧧🧧 LED Indicators
Red: System not connected to WiFi

Green: System connected and operating normally

Blue: Not currently used (available for custom status)
