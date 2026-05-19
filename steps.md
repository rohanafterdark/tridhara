Exact Steps to Connect Real Hardware
1. Wire the Sensors to ESP8266 NodeMCU
Parts: ESP8266 NodeMCU V3, DHT-11, YL-69 soil moisture + FC-28, LM-393 rain + FC-37, HC-SR04, breadboard, jumper wires, 1kΩ + 2kΩ resistors (voltage divider)
Connections:
Sensor	Pin
DHT-11 VCC	—
DHT-11 GND	—
DHT-11 DATA	—
FC-28 (soil) VCC	—
FC-28 GND	—
FC-28 AO	—
FC-37 (rain) VCC	—
FC-37 GND	—
FC-37 DO	—
HC-SR04 VCC	—
HC-SR04 GND	—
HC-SR04 Trig	—
HC-SR04 Echo	—
Voltage divider (mandatory — Echo outputs 5V, GPIO is 3.3V only):
HC-SR04 Echo ──[1kΩ]──┬──→ D5 (GPIO14)
                      │
                     [2kΩ]
                      │
                     GND
2. Install Arduino IDE Libraries
Open Sketch → Include Library → Manage Libraries, install:
- PubSubClient by Nick O'Leary
- DHT sensor library by Adafruit
- NewPingESP8266 (download ZIP from GitHub, then Sketch → Include Library → Add .ZIP Library)
Add ESP8266 board support:
- File → Preferences → Additional Boards Manager URLs: http://arduino.esp8266.com/stable/package_esp8266com_index.json
- Tools → Board → Boards Manager → search "ESP8266", install
- Select Tools → Board → ESP8266 Boards → NodeMCU 1.0 (ESP-12E Module)
3. Flash the Firmware
Open docs/IOT_FIRMWARE_TEMPLATE.ino in Arduino IDE. Change these 3 lines:
const char* WIFI_SSID     = "YOUR_WIFI_NAME";      // ← your WiFi
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";  // ← your password
const char* MQTT_SERVER   = "192.168.1.42";        // ← your laptop's IP
Find your laptop's IP:
# Linux/Mac
hostname -I
# Windows
ipconfig
Adjust tank depth if needed:
#define TANK_DEPTH_CM 150   // your tank depth in cm
Upload: Select the correct COM port under Tools → Port, then click Upload.
4. Start the Backend on Your Laptop
Open 3 terminals:
Terminal 1 — MQTT broker:
mosquitto -p 1883
Terminal 2 — Flask app:
python app.py
Terminal 3 — (optional) simulator for comparison:
python simulator/virtual_sensors.py --interval 2
5. Verify
Serial Monitor (Arduino IDE, 115200 baud):
[WiFi] IP: 192.168.1.xx
[MQTT] Connecting to broker... connected
[MQTT] Published 5 readings
Browser: http://localhost:5000/iot-demo.html
You should see 5 sensor cards with live updating values. The water_level card shows derived water height in cm.
6. Troubleshooting
Problem	Fix
MQTT connect failed	Check laptop firewall allows port 1883; ensure phone hotspot allows LAN devices
ping_cm() = 0	Check Trig/Echo not swapped; verify voltage divider; unobstructed sensor face
DHT read failed	Add 10kΩ pull-up between DATA and VCC; check wiring
No dashboard update	Open browser dev tools → Network tab, check /stream/sensors responds with data: lines
All values zero/stuck	Sensor probes corroded or loose Dupont connection
Multiple nodes: Change NODE_ID and MQTT_CLIENT_ID in the .ino for each additional ESP8266 (e.g., "delhi", "mumbai"). Each appears as a separate card.