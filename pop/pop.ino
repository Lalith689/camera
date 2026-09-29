#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// --- WI-FI STANDALONE ACCESS POINT CONFIGURATION ---
const char* AP_SSID = "SNYPTR-TARGET-RIG";
const char* AP_PASS = "snyptr1234";

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);
WiFiUDP udp;
const int UDP_PORT = 4210;

// --- PCA9685 SERVO DRIVER CONFIGURATION ---
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

const int SERVO_DOWN = 350;
const int SERVO_UP   = 150;

int currentPos[7] = {350, 350, 350, 350, 350, 350, 350};
String targetStates[7] = {"DOWN", "DOWN", "DOWN", "DOWN", "DOWN", "DOWN", "DOWN"};

void moveServoSmooth(int channel, int targetPos, int stepDelay = 3) {
  int startPos = currentPos[channel];
  if (startPos == targetPos) return;

  int step = (targetPos > startPos) ? 2 : -2;
  
  if (step > 0) {
    for (int p = startPos; p <= targetPos; p += step) {
      pwm.setPWM(channel, 0, p);
      delay(stepDelay);
    }
  } else {
    for (int p = startPos; p >= targetPos; p += step) {
      pwm.setPWM(channel, 0, p);
      delay(stepDelay);
    }
  }
  pwm.setPWM(channel, 0, targetPos);
  currentPos[channel] = targetPos;
  
  if (targetPos == SERVO_UP) {
    targetStates[channel] = "UP";
  } else {
    targetStates[channel] = "DOWN";
  }
}

void executeCommand(String line) {
  line.trim();
  if (line.startsWith("UP,")) {
    String idStr = line.substring(3);
    int targetId = idStr.toInt(); // 1 to 7
    if (targetId >= 1 && targetId <= 7) {
      int channel = targetId - 1;
      moveServoSmooth(channel, SERVO_UP, 3);
      Serial.print("[WIFI/SERVO] UP_CONFIRMED,");
      Serial.println(targetId);
    }
  }
  else if (line.startsWith("DOWN,")) {
    String idStr = line.substring(5);
    if (idStr == "ALL") {
      for (int channel = 0; channel < 7; channel++) {
        moveServoSmooth(channel, SERVO_DOWN, 3);
      }
      Serial.println("[WIFI/SERVO] DOWN_CONFIRMED,ALL");
    } else {
      int targetId = idStr.toInt(); // 1 to 7
      if (targetId >= 1 && targetId <= 7) {
        int channel = targetId - 1;
        moveServoSmooth(channel, SERVO_DOWN, 3);
        Serial.print("[WIFI/SERVO] DOWN_CONFIRMED,");
        Serial.println(targetId);
      }
    }
  }
}

void handleCmdRoute() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (server.hasArg("action")) {
    String action = server.arg("action");
    executeCommand(action);
    server.send(200, "application/json", "{\"status\":\"OK\",\"action\":\"" + action + "\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"Missing action param\"}");
  }
}

void handleTelemetryRoute() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String json = "{\"status\":\"READY\",\"targetState\":\"" + targetStates[0] + "\",\"targets\":[";
  for (int i = 0; i < 7; i++) {
    json += "\"" + targetStates[i] + "\"";
    if (i < 6) json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handlePingRoute() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "OK: PING_ACK (ESP32 WIFI TARGET RIG ONLINE)");
}

void setup() {
  Serial.begin(115200);

  // Initialize I2C for PCA9685
  Wire.begin(21, 22);
  pwm.begin();
  pwm.setPWMFreq(50);
  delay(200);

  // Start with all targets DOWN smoothly
  for (int channel = 0; channel < 7; channel++) {
    pwm.setPWM(channel, 0, SERVO_DOWN);
    currentPos[channel] = SERVO_DOWN;
    targetStates[channel] = "DOWN";
  }

  // --- START STANDALONE WIFI SOFT-AP ---
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.println("\n==================================================");
  Serial.println("  SNYPTR STANDALONE WIRELESS TARGET RIG ACTIVE");
  Serial.println("==================================================");
  Serial.print("  Wi-Fi Access Point : "); Serial.println(AP_SSID);
  Serial.print("  Wi-Fi Password     : "); Serial.println(AP_PASS);
  Serial.print("  Wi-Fi IP Address   : "); Serial.println(WiFi.softAPIP());
  Serial.println("==================================================");

  // HTTP Web Server Endpoints
  server.on("/cmd", handleCmdRoute);
  server.on("/telemetry", handleTelemetryRoute);
  server.on("/status", handleTelemetryRoute);
  server.on("/ping", handlePingRoute);
  server.begin();

  // UDP Wireless Listener
  udp.begin(UDP_PORT);
  Serial.println("[UDP] Wireless Listener running on port 4210.");
}

void loop() {
  // 1. Handle HTTP Web Server Requests
  server.handleClient();

  // 2. Handle Instant Wireless UDP Packets
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    char packetBuffer[128];
    int len = udp.read(packetBuffer, 127);
    if (len > 0) {
      packetBuffer[len] = 0;
      String msg = String(packetBuffer);
      executeCommand(msg);
    }
  }

  // 3. Fallback Serial Commands
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    executeCommand(line);
  }
}