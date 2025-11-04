#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ---------- WIFI ----------
const char* ssid = "Redmi 10";
const char* password = "qwerty1234";

// ---------- RFID ----------
const uint8_t RST_PIN = D1;
const uint8_t SS_PIN  = D2;
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ---------- LEDs & Buzzer ----------
const uint8_t GREEN_LED = D3;
const uint8_t RED_LED   = D4;
const uint8_t BUZZER    = D8;

// ---------- WEB SERVER ----------
ESP8266WebServer server(80);

// ---------- TRAY SETTINGS ----------
#define MAX_CARDS 50
String cardUIDs[MAX_CARDS];
bool sold[MAX_CARDS] = {false};
String lastScan[MAX_CARDS] = {""};
String currentScan[MAX_CARDS] = {""};
int cardCount = 0;
bool trayDone = false;
bool scanningActive = true;

// ---------- LOGIN ----------
const String USERNAME = "mpmcbatch";
const String PASSWORD = "mpmc";
bool loggedIn = false;

// ---------- NTP ----------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800);

// ---------- BUZZER CONTROL ----------
bool buzzerActive = false;
bool buzzerManuallyStopped = false;

// ---------- UTILITY ----------
String getCurrentTime() {
  timeClient.update();
  return timeClient.getFormattedTime();
}

int findCardIndex(String uid) {
  for (int i = 0; i < cardCount; i++) {
    if (cardUIDs[i] == uid) return i;
  }
  return -1;
}

// ---------- WEB HANDLERS ----------
void handleRoot() {
  if (!loggedIn) {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>💎 Jewelry Tracker Login</title>";
    html += "<style>"
            "body{background:linear-gradient(135deg,#dbeafe,#eef2ff);font-family:'Segoe UI',sans-serif;display:flex;justify-content:center;align-items:center;height:100vh;margin:0;}"
            ".login-box{background:white;padding:40px 45px;border-radius:15px;box-shadow:0 4px 25px rgba(0,0,0,0.1);width:320px;text-align:center;}"
            "h2{color:#1e3a8a;margin-bottom:25px;}"
            "input[type=text],input[type=password]{width:100%;padding:12px;margin:8px 0;border:1px solid #ccc;border-radius:8px;font-size:15px;}"
            ".eye-btn{position:relative;left:-35px;cursor:pointer;}"
            "input[type=submit]{background:#1e3a8a;color:white;padding:10px 20px;border:none;border-radius:8px;cursor:pointer;width:100%;font-size:15px;font-weight:600;}"
            "input[type=submit]:hover{background:#312e81;}"
            "</style></head><body>"
            "<div class='login-box'>"
            "<h2>💎 Jewelry Tracker Login</h2>"
            "<form method='POST' action='/login'>"
            "<input type='text' name='username' placeholder='Username' required><br>"
            "<div style='display:flex;align-items:center;justify-content:center;'>"
            "<input type='password' id='pwd' name='password' placeholder='Password' required>"
            "<span class='eye-btn' onclick='togglePwd()'>👁️</span></div>"
            "<input type='submit' value='Login'>"
            "</form></div>"
            "<script>"
            "function togglePwd(){var p=document.getElementById('pwd');p.type=(p.type==='password')?'text':'password';}"
            "</script></body></html>";
    server.send(200, "text/html", html);
    return;
  }

  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>💎 Jewelry Tracker Dashboard</title>";
  html += "<style>"
          "body{background:#f9fafb;font-family:'Segoe UI',sans-serif;text-align:center;margin:0;padding:25px;}"
          "h1{color:#1e3a8a;font-size:28px;margin-bottom:15px;}"
          ".dashboard{background:white;margin:auto;width:90%;max-width:900px;padding:25px;border-radius:15px;box-shadow:0 4px 20px rgba(0,0,0,0.08);}"
          "table{margin:auto;border-collapse:collapse;width:100%;border-radius:10px;overflow:hidden;}"
          "th{background:#1e3a8a;color:white;padding:12px;}"
          "td{border-bottom:1px solid #e5e7eb;padding:10px;}"
          "tr:nth-child(even){background:#f3f4f6;}"
          "button{padding:6px 14px;border:none;border-radius:6px;font-weight:600;cursor:pointer;}"
          ".found{background:#16a34a;color:white;}"
          ".sold{background:#f59e0b;color:white;}"
          ".missing{background:#dc2626;color:white;}"
          ".done,.end{background:#4f46e5;color:white;font-size:15px;padding:12px 30px;margin-top:25px;border:none;border-radius:8px;cursor:pointer;box-shadow:0 2px 6px rgba(0,0,0,0.15);}"
          ".done:hover,.end:hover{background:#3730a3;}"
          ".stop-btn{background:#dc2626;color:white;font-size:15px;padding:10px 25px;margin-top:20px;border:none;border-radius:8px;cursor:pointer;}"
          "p.alert{color:#dc2626;font-weight:bold;margin-top:20px;}"
          "</style>";
  html += "<script>"
          "function refreshStatus(){fetch('/status').then(r=>r.text()).then(d=>{document.getElementById('status').innerHTML=d;});}"
          "setInterval(refreshStatus,2000);"
          "function stopBuzzer(){fetch('/stop_buzzer').then(()=>{alert('Buzzer stopped');location.reload();});}"
          "</script></head><body>";
  html += "<h1>💎 Jewelry Tracker Dashboard</h1>";
  html += "<div class='dashboard'><div id='status'>Loading...</div>";
  if (scanningActive) {
    html += "<form action='/done' method='GET'><input type='submit' class='done' value='Finish Scanning'></form>";
  } else {
    html += "<form action='/end' method='GET'><input type='submit' class='end' value='Start New Scan'></form>";
  }
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleLogin() {
  if (!server.hasArg("username") || !server.hasArg("password")) {
    server.sendHeader("Location", "/"); server.send(303); return;
  }
  String u = server.arg("username");
  String p = server.arg("password");
  if (u == USERNAME && p == PASSWORD) {
    loggedIn = true;
    server.sendHeader("Location", "/"); server.send(303);
  } else {
    String html = "<!DOCTYPE html><html><body style='text-align:center;font-family:Arial;background:#f9fafb;'>"
                  "<h2 style='color:red;'>❌ Login Failed! Try Again</h2>"
                  "<a href='/' style='text-decoration:none;background:#1e3a8a;color:white;padding:10px 20px;border-radius:6px;'>Back to Login</a>"
                  "</body></html>";
    server.send(200, "text/html", html);
  }
}

void handleStatus() {
  if (!loggedIn) { server.sendHeader("Location", "/"); server.send(303); return; }

  String html = "";
  if (trayDone) {
    html += "<p style='font-size:18px;font-weight:bold;'>Total number of jewels found: " + String(cardCount) + "</p>";
    html += "<table><tr><th>Card UID</th><th>Status</th><th>Last Scan</th><th>Current Scan</th><th>Mark Sold</th></tr>";
    String missingCards = "";
    for (int i = 0; i < cardCount; i++) {
      html += "<tr><td>" + cardUIDs[i] + "</td>";
      String statusClass = "", statusText = "";
      if (sold[i]) { statusClass = "sold"; statusText = "SOLD"; }
      else if (currentScan[i] != "") { statusClass = "found"; statusText = "SCANNED"; }
      else { statusClass = "missing"; statusText = "MISSING"; missingCards += cardUIDs[i] + " "; }
      html += "<td><button class='" + statusClass + "'>" + statusText + "</button></td>";
      html += "<td>" + lastScan[i] + "</td><td>" + currentScan[i] + "</td>";
      if (!sold[i])
        html += "<td><form style='display:inline;' action='/mark_sold' method='GET'><input type='hidden' name='uid' value='" + cardUIDs[i] + "'><input type='submit' style='padding:6px 12px;background:#2563eb;color:white;border:none;border-radius:5px;cursor:pointer;' value='Mark Sold'></form></td>";
      else html += "<td></td>";
      html += "</tr>";
    }
    html += "</table>";
    if (missingCards != "" && !scanningActive) {
      html += "<p class='alert'>⚠️ Missing Cards: " + missingCards + "</p>";
      html += "<button class='stop-btn' onclick='stopBuzzer()'>Stop Buzzer</button>";
    }
  } else {
    html += "<p style='font-size:16px;color:#374151;'>🔄 Scanning in progress...</p>";
  }
  server.send(200, "text/html", html);
}

void handleMarkSold() {
  if (!server.hasArg("uid")) { server.send(400, "text/plain", "Missing UID"); return; }
  String uid = server.arg("uid");
  int idx = findCardIndex(uid);
  if (idx != -1) sold[idx] = true;
  server.sendHeader("Location", "/"); server.send(303);
}

void handleDone() {
  trayDone = true;
  scanningActive = false;
  server.sendHeader("Location", "/"); server.send(303);
}

void handleEnd() {
  scanningActive = true;
  for (int i = 0; i < cardCount; i++) currentScan[i] = "";
  trayDone = false;
  buzzerManuallyStopped = false;  // Reset buzzer state
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleStopBuzzer() {
  buzzerActive = false;
  buzzerManuallyStopped = true; // Prevent restart
  digitalWrite(BUZZER, LOW);
  digitalWrite(RED_LED, LOW);
  Serial.println("Buzzer manually stopped by user.");
  server.send(200, "text/plain", "Buzzer stopped");
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());

  timeClient.begin();

  server.on("/", handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/status", handleStatus);
  server.on("/mark_sold", handleMarkSold);
  server.on("/done", handleDone);
  server.on("/end", handleEnd);
  server.on("/stop_buzzer", handleStopBuzzer);

  server.begin();
  Serial.println("Web server started");
}

// ---------- LOOP ----------
void loop() {
  server.handleClient();

  bool missingExists = false;
  if (trayDone) {
    for (int i = 0; i < cardCount; i++) {
      if (!sold[i] && currentScan[i] == "") {
        missingExists = true;
        break;
      }
    }
  }

  // ✅ Continuous buzzer logic with manual stop check
  if (missingExists && !scanningActive && !buzzerManuallyStopped) {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER, HIGH);
    buzzerActive = true;
  } else if (!buzzerActive) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);
  }

  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  Serial.println("Scanned UID: " + uid);

  digitalWrite(GREEN_LED, HIGH);
  delay(200);
  digitalWrite(GREEN_LED, LOW);

  if (scanningActive && !trayDone) {
    int idx = findCardIndex(uid);
    if (idx == -1 && cardCount < MAX_CARDS) {
      cardUIDs[cardCount] = uid;
      cardCount++;
      Serial.println("Stored new UID: " + uid);
    }
  }

  int idx = findCardIndex(uid);
  if (idx != -1) {
    String now = getCurrentTime();
    if (currentScan[idx] == "") {
      currentScan[idx] = now;
      if (lastScan[idx] == "") lastScan[idx] = now;
    } else {
      lastScan[idx] = currentScan[idx];
      currentScan[idx] = now;
    }
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(1000);
}
