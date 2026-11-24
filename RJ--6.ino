
#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <DHT.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>

// ============================================================
// RJ ACTIVITY 6
// ESP32 + DHT11 + FIREBASE + DUAL RELAY
// NO SD CARD
// ============================================================

// FIREBASE CONFIGURATION
#define API_KEY "AIzaSyBeAHm65ky0p5mDpSMp6obm3FFcHIF-NCs"
#define USER_EMAIL "bebosorj2@gmail.com"
#define USER_PASSWORD "rjbeboso123"

#define DATABASE_URL \
"https://rjbeboso-5c489-default-rtdb.europe-west1.firebasedatabase.app"

// WIFI PORTAL - OPEN, NO PASSWORD
const char* AP_SSID = "RJ_Beboso_Setup";

// HARDWARE PINS
#define DHT_PIN 4
#define DHT_TYPE DHT11

#define RELAY1_PIN 26
#define RELAY2_PIN 27

// Relay module is ACTIVE LOW
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// TIMING
const unsigned long SAMPLE_INTERVAL_MS = 10000;

const long GMT_OFFSET_SEC = 8L * 60L * 60L;
const int DAYLIGHT_OFFSET_SEC = 0;

// OBJECTS
DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);
Preferences preferences;

WiFiClientSecure ssl_client;

using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);

UserAuth user_auth(
    API_KEY,
    USER_EMAIL,
    USER_PASSWORD
);

FirebaseApp app;
RealtimeDatabase Database;

// GLOBAL VARIABLES
bool relay1State = false;
bool relay2State = false;

unsigned long lastSampleMs = 0;

// ============================================================
// TIME FUNCTIONS
// GINAYA ANG TIME FORMAT NG ACTIVITY 4
// ============================================================

String getDateString() {
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo, 2000)) {
        return "1970-01-01";
    }

    char buffer[20];

    strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%d",
        &timeinfo
    );

    return String(buffer);
}

String getTimeString() {
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo, 2000)) {
        return "00:00:00";
    }

    char buffer[20];

    strftime(
        buffer,
        sizeof(buffer),
        "%H:%M:%S",
        &timeinfo
    );

    return String(buffer);
}

// ============================================================
// NTP TIME INITIALIZATION
// ============================================================

void initializeTime() {
    Serial.println();
    Serial.println("[TIME] Starting NTP synchronization...");

    configTime(
        GMT_OFFSET_SEC,
        DAYLIGHT_OFFSET_SEC,
        "pool.ntp.org",
        "time.nist.gov",
        "time.google.com"
    );

    Serial.print("[TIME] Waiting for time");

    struct tm timeinfo;
    int retry = 0;

    while (
        !getLocalTime(&timeinfo, 1000) &&
        retry < 30
    ) {
        Serial.print(".");
        delay(500);
        retry++;
    }

    Serial.println();

    if (
        getLocalTime(&timeinfo, 2000) &&
        timeinfo.tm_year >= 124
    ) {
        char dateBuffer[20];
        char timeBuffer[20];

        strftime(
            dateBuffer,
            sizeof(dateBuffer),
            "%Y-%m-%d",
            &timeinfo
        );

        strftime(
            timeBuffer,
            sizeof(timeBuffer),
            "%H:%M:%S",
            &timeinfo
        );

        Serial.println("[TIME] Synchronized!");
        Serial.print("[TIME] Date: ");
        Serial.println(dateBuffer);

        Serial.print("[TIME] Time: ");
        Serial.println(timeBuffer);
    } else {
        Serial.println("[TIME ERROR] Synchronization failed.");
    }
}

// ============================================================
// LITTLEFS INITIALIZATION
// ============================================================

void initializeLittleFS() {
    Serial.println("[LittleFS] Mounting filesystem...");

    if (!LittleFS.begin(true)) {
        Serial.println("[LittleFS ERROR] Mount failed.");

        while (true) {
            delay(1000);
        }
    }

    Serial.println("[LittleFS] Mounted successfully.");

    Serial.println(
        LittleFS.exists("/index.html")
        ? "[FILE] index.html FOUND"
        : "[FILE ERROR] index.html MISSING"
    );

    Serial.println(
        LittleFS.exists("/style.css")
        ? "[FILE] style.css FOUND"
        : "[FILE ERROR] style.css MISSING"
    );

    Serial.println(
        LittleFS.exists("/script.js")
        ? "[FILE] script.js FOUND"
        : "[FILE ERROR] script.js MISSING"
    );

    Serial.println(
        LittleFS.exists("/wifi.html")
        ? "[FILE] wifi.html FOUND"
        : "[FILE ERROR] wifi.html MISSING"
    );
}

// ============================================================
// WEBSITE FILE HANDLER
// ============================================================

void handleFile(String path, String contentType) {
    if (!LittleFS.exists(path)) {
        server.send(
            404,
            "text/plain",
            "File not found: " + path
        );

        Serial.println("[FILE ERROR] Missing: " + path);
        return;
    }

    File file = LittleFS.open(path, "r");

    server.streamFile(file, contentType);

    file.close();
}

// ============================================================
// WIFI PORTAL PAGE ROUTES
// ============================================================

void setupWiFiPortalRoutes() {

    server.on("/", HTTP_GET, []() {
        handleFile("/wifi.html", "text/html");
    });

    server.on("/wifi.html", HTTP_GET, []() {
        handleFile("/wifi.html", "text/html");
    });

    // SAVE WIFI CREDENTIALS
    server.on("/save", HTTP_POST, []() {

        String ssid = server.arg("ssid");
        String password = server.arg("pass");

        ssid.trim();

        if (ssid.length() == 0) {
            server.send(
                400,
                "text/plain",
                "WiFi SSID is required."
            );
            return;
        }

        if (
            password.length() > 0 &&
            password.length() < 8
        ) {
            server.send(
                400,
                "text/plain",
                "WiFi password must be at least 8 characters."
            );
            return;
        }

        if (password.length() > 63) {
            server.send(
                400,
                "text/plain",
                "WiFi password is too long."
            );
            return;
        }

        Serial.println();
        Serial.println("[WiFi] Saving credentials...");

        preferences.begin("rj-wifi", false);

        preferences.putString("ssid", ssid);
        preferences.putString("password", password);

        preferences.end();

        Serial.println("[WiFi] Credentials saved.");

        server.send(
            200,
            "text/html",
            "<!DOCTYPE html>"
            "<html><head>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>RJ WiFi Saved</title>"
            "</head>"
            "<body style='font-family:Arial;text-align:center;"
            "padding:35px;background:#0f172a;color:#38bdf8;'>"
            "<h2>WiFi Settings Saved!</h2>"
            "<p>RJ ESP32 is restarting.</p>"
            "<p>Please wait while it connects to your home WiFi.</p>"
            "<p>Check Serial Monitor for the new IP address.</p>"
            "</body></html>"
        );

        Serial.println("[WiFi] Restarting ESP32...");

        delay(2000);

        ESP.restart();
    });

    server.onNotFound([]() {

        server.sendHeader(
            "Location",
            "http://192.168.4.1/",
            true
        );

        server.send(
            302,
            "text/plain",
            ""
        );
    });

    server.begin();

    Serial.println("[WEB SERVER] WiFi portal started.");
}

// ============================================================
// START OPEN WIFI PORTAL
// ============================================================

void startWiFiPortal() {
    Serial.println();
    Serial.println("====================================");
    Serial.println("[WiFi] STARTING RJ WIFI MANAGER");
    Serial.println("====================================");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);

    delay(500);

    // OPEN ACCESS POINT - NO PASSWORD
    bool apStarted = WiFi.softAP(AP_SSID);

    if (!apStarted) {
        Serial.println("[WiFi ERROR] Cannot start access point.");

        delay(3000);
        ESP.restart();

        return;
    }

    Serial.println("[WiFi] Access Point Started.");

    Serial.print("[WiFi] Network Name: ");
    Serial.println(AP_SSID);

    Serial.println("[WiFi] Security: OPEN - NO PASSWORD");

    Serial.print("[WiFi] Setup IP: ");
    Serial.println(WiFi.softAPIP());

    setupWiFiPortalRoutes();

    Serial.println();
    Serial.println("====================================");
    Serial.println(" CONNECT TO RJ_Beboso_Setup");
    Serial.println(" OPEN http://192.168.4.1");
    Serial.println("====================================");

    while (true) {
        server.handleClient();
        delay(2);
    }
}

// ============================================================
// WIFI INITIALIZATION
// ============================================================

void initializeWiFi() {
    Serial.println();
    Serial.println("[WiFi] Checking saved credentials...");

    preferences.begin("rj-wifi", true);

    String savedSSID =
        preferences.getString("ssid", "");

    String savedPassword =
        preferences.getString("password", "");

    preferences.end();

    if (savedSSID.length() == 0) {
        Serial.println("[WiFi] No saved credentials.");

        startWiFiPortal();
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    Serial.print("[WiFi] Connecting to: ");
    Serial.println(savedSSID);

    WiFi.begin(
        savedSSID.c_str(),
        savedPassword.c_str()
    );

    unsigned long startTime = millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startTime < 20000
    ) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi ERROR] Connection failed.");
        Serial.println("[WiFi] Starting RJ WiFi Manager.");

        startWiFiPortal();
        return;
    }

    Serial.println("[WiFi] Connected successfully!");

    Serial.print("[WiFi] Home WiFi IP: ");
    Serial.println(WiFi.localIP());
}

// ============================================================
// FIREBASE INITIALIZATION
// ============================================================

void initializeFirebase() {
    Serial.println("[Firebase] Initializing...");

    ssl_client.setInsecure();

    initializeApp(
        aClient,
        app,
        getAuth(user_auth)
    );

    app.getApp<RealtimeDatabase>(Database);

    Database.url(DATABASE_URL);

    Serial.println("[Firebase] Initialization started.");
}

// ============================================================
// FIREBASE UPLOAD
// GINAGAMIT ANG getLocalTime() PARA SA DATE AT TIME
// ============================================================

bool uploadReading(
    float temperature,
    float humidity
) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Firebase] No WiFi connection.");
        return false;
    }

    if (!app.ready()) {
        Serial.println("[Firebase] Authentication not ready.");
        return false;
    }

    // GET ONE LOCAL TIME READING
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo, 2000)) {
        Serial.println("[TIME ERROR] Cannot get local time.");
        return false;
    }

    // CHECK IF NTP TIME IS VALID
    if (timeinfo.tm_year < 124) {
        Serial.println("[TIME ERROR] Invalid date and time.");
        return false;
    }

    // FORMAT DATE AND TIME FROM THE SAME TIME READING
    char dateBuffer[20];
    char timeBuffer[20];

    strftime(
        dateBuffer,
        sizeof(dateBuffer),
        "%Y-%m-%d",
        &timeinfo
    );

    strftime(
        timeBuffer,
        sizeof(timeBuffer),
        "%H:%M:%S",
        &timeinfo
    );

    String path =
        "/ESP32_Data/" +
        String(dateBuffer) +
        "/" +
        String(timeBuffer);

    String json =
        "{\"temperature\":" +
        String(temperature, 2) +
        ",\"humidity\":" +
        String(humidity, 2) +
        "}";

    bool success = Database.set<object_t>(
        aClient,
        path,
        object_t(json)
    );

    if (success) {
        Serial.println("[Firebase] Upload successful.");
        Serial.println("[Firebase] Path: " + path);

        return true;
    }

    Serial.printf(
        "[Firebase ERROR] Code: %d | %s\n",
        aClient.lastError().code(),
        aClient.lastError().message().c_str()
    );

    return false;
}

// ============================================================
// DHT11 SENSOR SAMPLING
// ============================================================

void sampleSensor() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (
        isnan(temperature) ||
        isnan(humidity)
    ) {
        Serial.println(
            "[DHT11 ERROR] Check sensor wiring."
        );

        return;
    }

    Serial.println();
    Serial.println("========== SENSOR READING ==========");

    Serial.printf(
        "Temperature: %.1f C\n",
        temperature
    );

    Serial.printf(
        "Humidity: %.1f %%\n",
        humidity
    );

    uploadReading(
        temperature,
        humidity
    );

    Serial.println("====================================");
}

// ============================================================
// RELAY CONTROL
// ============================================================

void applyRelayStates() {
    digitalWrite(
        RELAY1_PIN,
        relay1State ? RELAY_ON : RELAY_OFF
    );

    digitalWrite(
        RELAY2_PIN,
        relay2State ? RELAY_ON : RELAY_OFF
    );
}

void setRelay(
    int relayNumber,
    bool state
) {
    if (relayNumber == 1) {

        relay1State = state;

        digitalWrite(
            RELAY1_PIN,
            relay1State ? RELAY_ON : RELAY_OFF
        );

        Serial.printf(
            "[RELAY] LED 1: %s\n",
            relay1State ? "ON" : "OFF"
        );

    } else if (relayNumber == 2) {

        relay2State = state;

        digitalWrite(
            RELAY2_PIN,
            relay2State ? RELAY_ON : RELAY_OFF
        );

        Serial.printf(
            "[RELAY] LED 2: %s\n",
            relay2State ? "ON" : "OFF"
        );
    }
}

// ============================================================
// MAIN WEBSITE ROUTES
// ============================================================

void setupWebRoutes() {

    // MAIN DASHBOARD
    server.on("/", HTTP_GET, []() {
        handleFile("/index.html", "text/html");
    });

    // CSS
    server.on("/style.css", HTTP_GET, []() {
        handleFile("/style.css", "text/css");
    });

    // JAVASCRIPT
    server.on("/script.js", HTTP_GET, []() {
        handleFile(
            "/script.js",
            "application/javascript"
        );
    });

    // WIFI INFORMATION PAGE
    server.on("/wifi.html", HTTP_GET, []() {
        handleFile("/wifi.html", "text/html");
    });

    // RELAY STATUS
    server.on("/relay-status", HTTP_GET, []() {

        String json = "{";

        json += "\"LED1\":";
        json += relay1State ? "true" : "false";

        json += ",\"LED2\":";
        json += relay2State ? "true" : "false";

        json += "}";

        server.send(
            200,
            "application/json",
            json
        );
    });

    // RELAY COMMANDS
    server.on("/relay", HTTP_GET, []() {

        if (
            !server.hasArg("relay") ||
            !server.hasArg("state")
        ) {
            server.send(
                400,
                "application/json",
                "{\"success\":false,\"message\":\"Missing parameters\"}"
            );

            return;
        }

        int relayNumber =
            server.arg("relay").toInt();

        String state =
            server.arg("state");

        state.toLowerCase();

        if (
            relayNumber != 1 &&
            relayNumber != 2
        ) {
            server.send(
                400,
                "application/json",
                "{\"success\":false,\"message\":\"Invalid relay\"}"
            );

            return;
        }

        if (
            state != "on" &&
            state != "off"
        ) {
            server.send(
                400,
                "application/json",
                "{\"success\":false,\"message\":\"Invalid state\"}"
            );

            return;
        }

        bool turnOn = (state == "on");

        setRelay(
            relayNumber,
            turnOn
        );

        String json = "{\"success\":true,";

        json += "\"relay\":";
        json += String(relayNumber);

        json += ",\"state\":\"";
        json += state;
        json += "\"}";

        server.send(
            200,
            "application/json",
            json
        );
    });

    // UNKNOWN PAGES
    server.onNotFound([]() {

        server.send(
            404,
            "text/plain",
            "404 - Page not found"
        );
    });

    server.begin();

    Serial.println("[WEB SERVER] RJ Dashboard Started.");
}

// ============================================================
// SETUP
// ============================================================

void setup() {
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("====================================");
    Serial.println("       RJ ACTIVITY 6");
    Serial.println(" ESP32 + DHT11 + FIREBASE + RELAY");
    Serial.println(" NO SD CARD - LITTLEFS");
    Serial.println("====================================");

    // INITIALIZE RELAY PINS SAFELY
    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);

    relay1State = false;
    relay2State = false;

    applyRelayStates();

    Serial.println("[RELAY] Both LEDs initialized OFF.");

    // INITIALIZE DHT11
    dht.begin();

    Serial.println("[DHT11] Initialized.");

    // INITIALIZE LITTLEFS
    initializeLittleFS();

    // CONNECT WIFI OR START OPEN WIFI PORTAL
    initializeWiFi();

    // SYNCHRONIZE PHILIPPINES TIME
    initializeTime();

    // INITIALIZE FIREBASE
    initializeFirebase();

    // START MAIN WEBSITE
    setupWebRoutes();

    Serial.println();
    Serial.println("====================================");
    Serial.println("[SYSTEM] RJ ACTIVITY 6 READY");

    Serial.print("[WEBSITE] http://");
    Serial.println(WiFi.localIP());

    Serial.println("[FIREBASE] Monitoring enabled.");
    Serial.println("[RELAY] GPIO 26 and GPIO 27.");

    Serial.println("====================================");
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {

    // FIREBASE BACKGROUND PROCESSING
    app.loop();

    // HANDLE WEBSITE REQUESTS
    server.handleClient();

    // RECONNECT WIFI IF DISCONNECTED
    if (WiFi.status() != WL_CONNECTED) {

        static unsigned long lastReconnectMs = 0;

        if (millis() - lastReconnectMs >= 10000) {

            lastReconnectMs = millis();

            Serial.println("[WiFi] Reconnecting...");

            WiFi.reconnect();
        }
    }

    // SAMPLE DHT11 EVERY 10 SECONDS
    if (
        millis() - lastSampleMs >= SAMPLE_INTERVAL_MS
    ) {
        lastSampleMs = millis();

        if (WiFi.status() == WL_CONNECTED) {
            sampleSensor();
        } else {
            Serial.println(
                "[WiFi] Offline. Sensor upload skipped."
            );
        }
    }

    delay(2);
}
