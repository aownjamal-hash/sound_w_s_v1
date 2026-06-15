#include <Arduino.h>
#include <Preferences.h>
#include "web_page.h"
#include <WebServer.h>
#include <TimeLib.h>
setup
// تعريف الدبابيس
const int SENSOR_PIN = 34;
const int BUTTON_PIN = 13;
const int RESET_BUTTON_PIN = 0;
const int LED_PIN = 25;
const int LED_NETWORK_PIN = 2;
const int WIFI_BUTTON_PIN = 14;
const int LED_WIFI_PIN = 27;
#define RX_PIN 17
#define TX_PIN 16

// متغيرات النظام
int idleBase = 2000;
int threshold = 2200;
int counter = 0;
volatile bool smsEnabled = false;
volatile bool networkReady = false;
String currentDateTime = "";
int lastSoundValue = 0;

unsigned long lastCountTime = 0;
String inputBuffer = "";
unsigned long lastNetworkLedToggle = 0;
bool networkLedState = false;

// أزرار
unsigned long lastButtonReleaseTime = 0;
int buttonPressCount = 0;
bool lastButtonState = HIGH;

// زر RESET (GPIO0)
unsigned long lastResetPressTime = 0;
bool lastResetState = HIGH;
bool reinitInProgress = false;
unsigned long reinitStepStart = 0;
int reinitStep = 0;
unsigned long networkWaitStart = 0;
const unsigned long NETWORK_TIMEOUT_MS = 20000;

// الواي فاي
bool wifiEnabled = true;
unsigned long lastWifiButtonTime = 0;
bool lastWifiButtonState = HIGH;

// إدارة الوقت
time_t bootEpoch = 0;
unsigned long bootMillis = 0;
int signalStrength = 0;

String myPhoneNumber = "";
String messageText = "";
WebServer server(80);
Preferences preferences;

// إعلانات الدوال
bool checkNetworkRegistration();
String fetchNetworkTime();
String getCurrentTimeString();
time_t parseNetworkTimeToEpoch(String timeStr);
int getSignalStrength();
void updateNetworkAndSignal();
void toggleWiFi();
void updateWifiLED();
void sendAlertSMS(int currentCount, int soundVal);
void calibrate();
void loadStoredSettings();
bool waitForNetworkWithTimeout(unsigned long timeoutMs);
void startReinitialization();
void handleReinitialization();
void parseGSMStandardLine(String line);
void handleRoot();
void handleSave();
void handleState();
void handleSetMode();
void handleCalibrate();
void handleSetThreshold();
void handleUpdateNetwork();
void handleSyncTime();
void handleReconnectNetwork();
void handleToggleWiFi();
void addToLog(String event);   // إضافة دالة السجل البسيطة

// ========== سجل بسيط (بدون تخزين دائم) ==========
void addToLog(String event) {
    String timestamp = getCurrentTimeString();
    String entry = timestamp + " - " + event + "\n";
    
    Serial.print("[LOG] " + entry);

    // Append to Flash File
    File file = LITTLEFS.open("/events.txt", FILE_APPEND);
    if(file) {
        if(file.print(entry)) {
            // Success
        }
        file.close();
    } else {
        Serial.println("[LOG] Failed to open log file");
    }
}

void handleViewLogs() {
    File file = LITTLEFS.open("/events.txt", FILE_READ);
    if(!file) {
        server.send(200, "text/plain", "No logs found.");
        return;
    }
    server.streamFile(file, "text/plain");
    file.close();
}

// In setup():
server.on("/viewlogs", HTTP_GET, handleViewLogs);
server.on("/clearlogs", HTTP_POST, []() {
    LITTLEFS.remove("/events.txt");
    server.send(200, "text/plain", "Logs cleared.");
});
// ========== دوال LED الأساسية ==========
void blinkStatus(int times, int duration) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(duration);
        digitalWrite(LED_PIN, LOW);
        delay(duration);
    }
}

void blinkNetworkLed(int times, int duration) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_NETWORK_PIN, HIGH);
        delay(duration);
        digitalWrite(LED_NETWORK_PIN, LOW);
        delay(duration);
    }
}

void updateWifiLED() {
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        digitalWrite(LED_WIFI_PIN, HIGH);
    } else {
        digitalWrite(LED_WIFI_PIN, LOW);
    }
}

void toggleWiFi() {
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        WiFi.mode(WIFI_OFF);
        wifiEnabled = false;
        addToLog("WiFi AP turned OFF");
    } else {
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32", "12345678", 1, true);
        wifiEnabled = true;
        addToLog("WiFi AP turned ON");
    }
    updateWifiLED();
}

// ========== دوال الوقت ==========
String getCurrentTimeString() {
    if (bootEpoch == 0) return "2024-01-01 00:00:00";
    time_t now = bootEpoch + ((millis() - bootMillis) / 1000);
    char buf[20];
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
        year(now), month(now), day(now),
        hour(now), minute(now), second(now));
    return String(buf);
}

time_t parseNetworkTimeToEpoch(String timeStr) {
    if (timeStr.length() < 19) return 0;
    int year = timeStr.substring(0,4).toInt();
    int month = timeStr.substring(5,7).toInt();
    int day = timeStr.substring(8,10).toInt();
    int hour = timeStr.substring(11,13).toInt();
    int minute = timeStr.substring(14,16).toInt();
    int second = timeStr.substring(17,19).toInt();
    struct tm tmStruct;
    tmStruct.tm_year = year - 1900;
    tmStruct.tm_mon = month - 1;
    tmStruct.tm_mday = day;
    tmStruct.tm_hour = hour;
    tmStruct.tm_min = minute;
    tmStruct.tm_sec = second;
    tmStruct.tm_isdst = 0;
    return mktime(&tmStruct);
}

String fetchNetworkTime() {
    for (int attempt = 0; attempt < 6; attempt++) {
        Serial2.println("AT+CCLK?");
        delay(600);
        String reply = "";
        while (Serial2.available()) {
            char c = Serial2.read();
            if (c == '\n') {
                if (reply.indexOf("+CCLK:") != -1) {
                    int start = reply.indexOf('"');
                    int end = reply.lastIndexOf('"');
                    if (start != -1 && end > start) {
                        String raw = reply.substring(start + 1, end);
                        if (raw.length() >= 17) {
                            int year = raw.substring(0,2).toInt();
                            if (year >= 23) {
                                String fullYear = "20" + String(year);
                                String month = raw.substring(3,5);
                                String day = raw.substring(6,8);
                                String hour = raw.substring(9,11);
                                String minute = raw.substring(12,14);
                                String second = raw.substring(15,17);
                                return fullYear + "-" + month + "-" + day + " " + hour + ":" + minute + ":" + second;
                            }
                        }
                    }
                }
                reply = "";
            } else if (c >= 32 && c <= 126) reply += c;
        }
        delay(1500);
    }
    return "2024-01-01 00:00:00";
}

// ========== إدارة الشبكة ==========
bool checkNetworkRegistration() {
    Serial2.println("AT+CREG?");
    delay(500);
    String reply = "";
    while (Serial2.available()) {
        char c = Serial2.read();
        if (c == '\n') {
            if (reply.indexOf("+CREG:") != -1) {
                if (reply.indexOf(",1") != -1 || reply.indexOf(",5") != -1) return true;
            }
            reply = "";
        } else if (c >= 32 && c <= 126) reply += c;
    }
    return false;
}

int getSignalStrength() {
    Serial2.println("AT+CSQ");
    delay(300);
    String reply = "";
    while (Serial2.available()) {
        char c = Serial2.read();
        if (c == '\n') {
            if (reply.indexOf("+CSQ:") != -1) {
                int comma = reply.indexOf(',');
                if (comma != -1) {
                    String rssiStr = reply.substring(reply.indexOf(':')+1, comma);
                    rssiStr.trim();
                    int rssi = rssiStr.toInt();
                    if (rssi == 99) return 0;
                    int percent = map(rssi, 0, 31, 0, 100);
                    if (percent > 100) percent = 100;
                    return percent;
                }
            }
            reply = "";
        } else if (c >= 32 && c <= 126) reply += c;
    }
    return 0;
}

void updateNetworkAndSignal() {
    networkReady = checkNetworkRegistration();
    signalStrength = getSignalStrength();
    if (networkReady && getCurrentTimeString() == "2024-01-01 00:00:00") {
        // محاولة جلب الوقت تلقائياً بعد نجاح الاتصال
        for (int i = 0; i < 3; i++) {
            String timeStr = fetchNetworkTime();
            if (timeStr != "2024-01-01 00:00:00") {
                bootEpoch = parseNetworkTimeToEpoch(timeStr);
                bootMillis = millis();
                currentDateTime = getCurrentTimeString();
                addToLog("Time synced after network update: " + currentDateTime);
                break;
            }
            delay(1500);
        }
    }
}

void reconnectNetwork() {
    addToLog("Manual network reconnection requested");
    Serial.println("[NETWORK] Manual reconnection...");
    Serial2.println("AT+CFUN=1,1");
    delay(3000);
    Serial2.println("AT+COPS=0");
    delay(5000);
    
    networkReady = checkNetworkRegistration();
    signalStrength = getSignalStrength();
    
    if (networkReady) {
        addToLog("Network reconnected, signal=" + String(signalStrength));
        for (int i = 0; i < 3; i++) {
            String timeStr = fetchNetworkTime();
            if (timeStr != "2024-01-01 00:00:00") {
                bootEpoch = parseNetworkTimeToEpoch(timeStr);
                bootMillis = millis();
                currentDateTime = getCurrentTimeString();
                addToLog("Time synced after reconnect: " + currentDateTime);
                break;
            }
            delay(1500);
        }
    } else {
        addToLog("Network reconnection failed");
    }
}

// ========== إرسال SMS ==========
String encodeToHEX(String text) {
    String hexResult = "";
    int i = 0;
    while (i < text.length()) {
        uint16_t unicodeChar = 0;
        uint8_t c = text[i];
        if (c < 0x80) {
            unicodeChar = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            unicodeChar = ((c & 0x1F) << 6) | (text[i+1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            unicodeChar = ((c & 0x0F) << 12) | ((text[i+1] & 0x3F) << 6) | (text[i+2] & 0x3F);
            i += 3;
        } else {
            i++;
            continue;
        }
        char buf[5];
        sprintf(buf, "%04X", unicodeChar);
        hexResult += buf;
    }
    return hexResult;
}

void sendAlertSMS(int currentCount, int soundVal) {
    if (!smsEnabled || !networkReady) return;

    String msg = messageText;
    if (msg.length() == 0) msg = "هلا فتحت البوابه\n";
    else msg += "\n";
    msg += "التاريخ والوقت: " + getCurrentTimeString() + "\n";
    msg += "رقم العدة: " + String(currentCount) + "\n";
    msg += "قيمة الصوت: " + String(soundVal);

    Serial.println("\n[GSM] Preparing SMS...");
    Serial2.println("AT+CSMP=17,167,0,8");
    delay(300);

    String hexMessage = encodeToHEX(msg);
    Serial2.print("AT+CMGS=\"");
    Serial2.print(myPhoneNumber);
    Serial2.println("\"");
    delay(1000);

    Serial2.print(hexMessage);
    delay(500);
    Serial2.write(26);
    delay(4000);
    Serial.println("[GSM] SMS sent.");
    addToLog("SMS sent: Count=" + String(currentCount));
}

// ========== المعايرة والإعدادات ==========
void calibrate() {
    long sum = 0;
    const int samples = 100;
    for (int i = 0; i < samples; i++) {
        sum += analogRead(SENSOR_PIN);
        delay(2);
    }
    idleBase = sum / samples;
    threshold = idleBase + 150;
    preferences.begin("sound_config", false);
    preferences.putInt("storedBase", idleBase);
    preferences.putInt("storedThresh", threshold);
    preferences.end();
    addToLog("Calibration done: Base=" + String(idleBase) + " Th=" + String(threshold));
}

void loadStoredSettings() {
    preferences.begin("sound_config", true);
    idleBase = preferences.getInt("storedBase", 2000);
    threshold = preferences.getInt("storedThresh", 2150);
    myPhoneNumber = preferences.getString("phone", "+967772274423");
    messageText = preferences.getString("msg", "");
    preferences.end();
    Serial.printf("[LOAD] Base=%d Th=%d\n", idleBase, threshold);
}

bool waitForNetworkWithTimeout(unsigned long timeoutMs) {
    Serial.println("[NETWORK] Waiting...");
    unsigned long startTime = millis();
    while (millis() - startTime < timeoutMs) {
        if (checkNetworkRegistration()) {
            Serial.println("[NETWORK] Registered.");
            delay(3000);
            String timeStr = fetchNetworkTime();
            bootEpoch = parseNetworkTimeToEpoch(timeStr);
            bootMillis = millis();
            currentDateTime = getCurrentTimeString();
            networkReady = true;
            signalStrength = getSignalStrength();
            if (!smsEnabled) {
                smsEnabled = true;
                blinkStatus(2,300);
            }
            addToLog("Network registered, signal=" + String(signalStrength));
            return true;
        }
        delay(1000);
        digitalWrite(LED_NETWORK_PIN, !digitalRead(LED_NETWORK_PIN));
    }
    networkReady = false;
    addToLog("Network registration timeout");
    return false;
}

void startReinitialization() {
    if (reinitInProgress) return;
    reinitInProgress = true;
    reinitStep = 1;
    reinitStepStart = millis();
    Serial2.println("AT+CFUN=0");
    delay(200);
    while(Serial2.available()) Serial2.read();
}

void handleReinitialization() {
    if (!reinitInProgress) return;
    switch (reinitStep) {
        case 1:
            if (millis() - reinitStepStart > 1000) {
                Serial2.println("AT+CFUN=1");
                reinitStep = 2;
                reinitStepStart = millis();
            }
            break;
        case 2:
            if (millis() - reinitStepStart > 3000) {
                networkWaitStart = millis();
                reinitStep = 3;
            }
            break;
        case 3:
            if (checkNetworkRegistration()) {
                delay(2000);
                String timeStr = fetchNetworkTime();
                bootEpoch = parseNetworkTimeToEpoch(timeStr);
                bootMillis = millis();
                currentDateTime = getCurrentTimeString();
                networkReady = true;
                signalStrength = getSignalStrength();
                blinkNetworkLed(5,100);
                reinitInProgress = false;
                reinitStep = 0;
                lastNetworkLedToggle = millis();
                addToLog("Modem reinitialized (BOOT button)");
            } else if (millis() - networkWaitStart > NETWORK_TIMEOUT_MS) {
                networkReady = false;
                reinitInProgress = false;
                reinitStep = 0;
                digitalWrite(LED_NETWORK_PIN, LOW);
                addToLog("Manual reinit failed");
            }
            break;
    }
}

void parseGSMStandardLine(String line) {
    line.trim();
    if (line.length() == 0) return;
    if (line.indexOf("RING") != -1) Serial.println("[CALL] Ringing...");
    else if (line.indexOf("NO CARRIER") != -1) Serial.println("[STBY] Call ended.");
}

// ========== دوال API للويب ==========
void handleState() {
    // إذا كانت الشبكة جاهزة والوقت افتراضي، نحاول المزامنة
    if (networkReady && getCurrentTimeString() == "2024-01-01 00:00:00") {
        for (int i = 0; i < 3; i++) {
            String timeStr = fetchNetworkTime();
            if (timeStr != "2024-01-01 00:00:00") {
                bootEpoch = parseNetworkTimeToEpoch(timeStr);
                bootMillis = millis();
                currentDateTime = getCurrentTimeString();
                addToLog("Time auto-synced via state request");
                break;
            }
            delay(1000);
        }
    }
    String phoneForDisplay = myPhoneNumber;
    if (phoneForDisplay.startsWith("+967")) phoneForDisplay = phoneForDisplay.substring(4);
    String json = "{";
    json += "\"networkReady\":" + String(networkReady ? "true" : "false") + ",";
    json += "\"signalStrength\":" + String(signalStrength) + ",";
    json += "\"currentDateTime\":\"" + getCurrentTimeString() + "\",";
    json += "\"smsEnabled\":" + String(smsEnabled ? "true" : "false") + ",";
    json += "\"threshold\":" + String(threshold) + ",";
    json += "\"idleBase\":" + String(idleBase) + ",";
    json += "\"lastSound\":" + String(lastSoundValue) + ",";
    json += "\"counter\":" + String(counter) + ",";
    json += "\"phone\":\"" + phoneForDisplay + "\",";
    json += "\"msg\":\"" + messageText + "\",";
    json += "\"wifiEnabled\":" + String((WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
}

void handleUpdateNetwork() {
    updateNetworkAndSignal();
    server.send(200, "text/plain", "Updated");
}

void handleSyncTime() {
    if (!networkReady) {
        server.send(400, "text/plain", "Network not ready. Update network first.");
        return;
    }
    String timeStr = "";
    for (int i = 0; i < 3; i++) {
        timeStr = fetchNetworkTime();
        if (timeStr != "2024-01-01 00:00:00") break;
        delay(1500);
    }
    if (timeStr != "2024-01-01 00:00:00") {
        bootEpoch = parseNetworkTimeToEpoch(timeStr);
        bootMillis = millis();
        currentDateTime = getCurrentTimeString();
        addToLog("Manual time sync: " + currentDateTime);
        server.send(200, "text/plain", "Time synced: " + currentDateTime);
    } else {
        server.send(500, "text/plain", "Sync failed. Check modem and network.");
    }
}

void handleReconnectNetwork() {
    reconnectNetwork();
    server.send(200, "text/plain", networkReady ? "Network reconnected" : "Reconnection failed");
}

void handleToggleWiFi() {
    toggleWiFi();
    server.send(200, "text/plain", wifiEnabled ? "WiFi ON" : "WiFi OFF");
}

void handleSetMode() {
    smsEnabled = !smsEnabled;
    if(smsEnabled) blinkStatus(2,300);
    else blinkStatus(5,80);
    addToLog(smsEnabled ? "SMS armed" : "SMS muted");
    server.send(200, "text/plain", smsEnabled ? "armed" : "mute");
}

void handleCalibrate() {
    calibrate();
    server.send(200, "text/plain", "Calibration done");
}

void handleSetThreshold() {
    if(server.hasArg("idleBase")) idleBase = server.arg("idleBase").toInt();
    if(server.hasArg("threshold")) threshold = server.arg("threshold").toInt();
    preferences.begin("sound_config", false);
    preferences.putInt("storedBase", idleBase);
    preferences.putInt("storedThresh", threshold);
    preferences.end();
    addToLog("Threshold updated: Base=" + String(idleBase) + " Th=" + String(threshold));
    server.send(200, "text/plain", "ok");
}

void handleSave() {
    if(server.hasArg("phone")) {
        String rawPhone = server.arg("phone");
        rawPhone.trim();
        if (rawPhone.startsWith("+")) rawPhone = rawPhone.substring(1);
        myPhoneNumber = "+967" + rawPhone;
    }
    if(server.hasArg("msg")) messageText = server.arg("msg");
    preferences.begin("sound_config", false);
    preferences.putString("phone", myPhoneNumber);
    preferences.putString("msg", messageText);
    preferences.end();
    addToLog("Settings saved: Phone=" + myPhoneNumber);
    server.send(200, "text/html", "تم الحفظ! <a href='/'>عودة</a>");
}

void handleRoot() {
    String page = FPSTR(index_html);
    String phoneForDisplay = myPhoneNumber;
    if (phoneForDisplay.startsWith("+967")) phoneForDisplay = phoneForDisplay.substring(4);
    page.replace("%PHONE%", phoneForDisplay);
    page.replace("%MSG%", messageText);
    server.send(200, "text/html", page);
}

// ========== الإعداد ==========
void setup() {
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

    pinMode(SENSOR_PIN, INPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    pinMode(WIFI_BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    pinMode(LED_NETWORK_PIN, OUTPUT);
    pinMode(LED_WIFI_PIN, OUTPUT);
    digitalWrite(LED_NETWORK_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    delay(4000);
    addToLog("System started");
    loadStoredSettings();

    // تهيئة المودم
    Serial2.println("AT"); delay(400);
    Serial2.println("AT+CLIP=1"); delay(400);
    Serial2.println("AT+CMGF=1"); delay(400);
    Serial2.println("AT+CSCS=\"HEX\""); delay(400);
    Serial2.println("AT+CNMI=2,2,0,0,0"); delay(400);
    Serial2.println("AT+CTZU=1"); delay(300);
    Serial2.println("AT+CLTS=1"); delay(300);
    while (Serial2.available()) Serial2.read();

    waitForNetworkWithTimeout(NETWORK_TIMEOUT_MS);

    // إعداد الواي فاي
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32", "12345678", 1, true);
    wifiEnabled = true;
    updateWifiLED();

    // إعداد خادم الويب
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/state", handleState);
    server.on("/setMode", HTTP_POST, handleSetMode);
    server.on("/calibrate", HTTP_POST, handleCalibrate);
    server.on("/setThreshold", HTTP_POST, handleSetThreshold);
    server.on("/updatenetwork", HTTP_POST, handleUpdateNetwork);
    server.on("/synctime", HTTP_POST, handleSyncTime);
    server.on("/reconnect", HTTP_POST, handleReconnectNetwork);
    server.on("/toggleWiFi", HTTP_POST, handleToggleWiFi);
    server.begin();

    Serial.println("System ready. WiFi: ESP32 / 12345678 | IP: 192.168.4.1");
    Serial.println("GPIO13: toggle SMS mode / double press calibrate");
    Serial.println("GPIO0: reinitialize modem");
    Serial.println("GPIO14: toggle WiFi | GPIO27: WiFi status LED");

    if(!LITTLEFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
    }
}

// ========== الحلقة الرئيسية ==========
void loop() {
    server.handleClient();
    handleReinitialization();

    // وميض LED الشبكة
    if (networkReady && !reinitInProgress) {
        if (millis() - lastNetworkLedToggle >= 1000) {
            lastNetworkLedToggle = millis();
            networkLedState = !networkLedState;
            digitalWrite(LED_NETWORK_PIN, networkLedState);
        }
    } else if (!reinitInProgress) {
        digitalWrite(LED_NETWORK_PIN, LOW);
    }

    // زر 13 (تبديل الصامت / معايرة)
    int btn = digitalRead(BUTTON_PIN);
    if (btn == LOW && lastButtonState == HIGH) {
        delay(20);
        if (digitalRead(BUTTON_PIN) == LOW) {
            buttonPressCount++;
            lastButtonReleaseTime = millis();
        }
    }
    lastButtonState = btn;
    if (buttonPressCount > 0 && (millis() - lastButtonReleaseTime > 350)) {
        if (buttonPressCount == 1) {
            smsEnabled = !smsEnabled;
            if (smsEnabled) blinkStatus(2,300);
            else blinkStatus(5,80);
            addToLog(smsEnabled ? "SMS armed" : "SMS muted");
        } else if (buttonPressCount >= 2) {
            digitalWrite(LED_PIN, HIGH);
            calibrate();
            digitalWrite(LED_PIN, LOW);
            blinkStatus(1,300);
        }
        buttonPressCount = 0;
    }

    // زر الواي فاي (GPIO14)
    int wifiBtn = digitalRead(WIFI_BUTTON_PIN);
    if (wifiBtn == LOW && lastWifiButtonState == HIGH) {
        delay(50);
        if (digitalRead(WIFI_BUTTON_PIN) == LOW) {
            toggleWiFi();
            blinkStatus(1, 200);
        }
    }
    lastWifiButtonState = wifiBtn;

    // زر BOOT (GPIO0)
    int resetBtn = digitalRead(RESET_BUTTON_PIN);
    if (resetBtn == LOW && lastResetState == HIGH) {
        delay(50);
        if (digitalRead(RESET_BUTTON_PIN) == LOW && !reinitInProgress)
            startReinitialization();
    }
    lastResetState = resetBtn;

    // قراءة الردود من المودم
    while (Serial2.available()) {
        char c = Serial2.read();
        if (c == '\n') {
            parseGSMStandardLine(inputBuffer);
            inputBuffer = "";
        } else if (c >= 32 && c <= 126) inputBuffer += c;
    }

    // تحديث الوقت كل ساعة (إن أمكن)
    static unsigned long lastTimeUpdate = 0;
    if (networkReady && (millis() - lastTimeUpdate > 3600000)) {
        String newTime = fetchNetworkTime();
        if (newTime != "2024-01-01 00:00:00") {
            bootEpoch = parseNetworkTimeToEpoch(newTime);
            bootMillis = millis();
            currentDateTime = getCurrentTimeString();
            addToLog("Hourly time sync: " + currentDateTime);
        }
        lastTimeUpdate = millis();
    }

    // قراءة حساس الصوت
    int val = analogRead(SENSOR_PIN);
    lastSoundValue = val;
    unsigned long cooldown = smsEnabled ? 4000 : 300;
    if (val > threshold && (millis() - lastCountTime > cooldown)) {
        counter++;
        digitalWrite(LED_PIN, HIGH);
        if (smsEnabled && networkReady) {
            Serial.printf("[ARMED] Count=%d | Val=%d | Time=%s\n", counter, val, getCurrentTimeString().c_str());
            sendAlertSMS(counter, val);
        } else if (!smsEnabled) {
            Serial.printf("[MUTE] Count=%d | Val=%d\n", counter, val);
        }
        lastCountTime = millis();
    }
    if (millis() - lastCountTime > (smsEnabled ? 150 : 60))
        digitalWrite(LED_PIN, LOW);

    if (Serial.available()) Serial2.write(Serial.read());
}




