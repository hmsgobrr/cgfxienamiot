#include <SPI.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "RTClib.h"

#include "firebase.h"
#include "tds.h"
#include "ph.h"

#define MAXJAM 50               // maksimal 50 waktu penyiraman berbeda dalam sehari
#define REL 5                   // PIN RELAY
#define TOL 15                  // Toleransi telat meniram (menit)
#define INTERVAL_AMBIL 60       // Akan ngambil data (interval + waktu nyiram) tiap 60 detik
#define INTERVAL_UPDATE 5      // Akan ngambil data (interval + waktu nyiram) tiap 60 detik


bool isPumpOn = false;
unsigned long pumpStartTime = 0;

RTC_DS3231 rtc;
WiFiClient client;
HTTPClient http;
DateTime now;

char jams[7][MAXJAM];
int lenj = 0;
int interval = 10;

unsigned long lastUpdate = millis(), lastAmbil;

String httpGETRequest(const char* serverName) {
    // Your Domain name with URL path or IP address with path
    http.begin(client, serverName);

    // If you need Node-RED/server authentication, insert user and password below
    //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

    // Send HTTP POST request
    int httpResponseCode = http.GET();

    String payload = "{}"; 

    if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        payload = http.getString();
    }
    else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();

    return payload;
}

void update() {
    if(WiFi.status()== WL_CONNECTED){
        /// GET: WAKTU-WAKTU NYIRAM
        String gete = httpGETRequest(GET_TIMES);
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, gete.c_str());
        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return;
        }
        JsonArray values = doc["fields"]["times"]["arrayValue"]["values"].as<JsonArray>();
        int i = 0;
        for (JsonVariant v : values) {
            const char* strval = v["stringValue"];
            for (int j = 0; j < 5; j++) {   
                jams[i][j] = strval[j];
            }
            i++;
        }

        /// GET: INTERVAL NYIRAM
        gete = httpGETRequest(GET_TIMES);
        error = deserializeJson(doc, gete.c_str());
        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return;
        }
        interval = doc["fields"]["interval"]["integerValue"].as<int>();
    }
    else {
        Serial.println("WiFi Disconnected");
    }
}

void post() {
    if (WiFi.status() == WL_CONNECTED) {
        // Your Domain name with URL path or IP address with path
        http.begin(client, POST_DATA);

        // If you need Node-RED/server authentication, insert user and password below
        //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

        String time = "";
        if (now.hour() < 10) time += "0";
        time += now.hour();
        time += ":";
        if (now.minute() < 10) time += "0";
        time += now.minute();
        time += " ";
        time += now.day();
        time += "/";
        time += now.month();
        time += "/";
        time += now.year();

        JsonDocument doc;
        doc["fields"]["ph"] = getPH();
        doc["fields"]["tds"] = gettds();
        doc["fields"]["temp"] = getTMP();
        doc["fields"]["time"] = time.c_str();
        String jeson;
        serializeJson(doc, jeson);

        http.addHeader("Content-Type", "application/json");
        int httpResponseCode = http.POST(jeson);

        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        // Free resources
        http.end();
    } else {
        Serial.println("WiFi Disconnected");
    }
}

void setup() {
    Serial.begin(115200);
    
    WiFi.begin(SSID, PASS);
    Serial.println("Connecting");
    while(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());

    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        while (1) delay(300);
    }

    if (rtc.lostPower()) {
        Serial.println("RTC lost power, let's set the time!");
    }
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    update();
    lastAmbil = millis();
}

// String stime(DateTime& now) {
//     return now.hour() + ":" + now.minute() + ":" + now.second() + " " + now.day() + "/" + now.month() + "/" + now.year();
// }

int getHour(const char* str) {
    String a;
    a += str[0];
    a += str[1];

    return a.toInt();
}
int getMint(const char* str) {
    String a;
    a += str[3];
    a += str[4];

    return a.toInt();
}

void loop() {
    looptds();
    now = rtc.now();

    if ((millis() - lastAmbil) >= INTERVAL_AMBIL*1000) {
        update();
        lastAmbil = millis();
    }
    if ((millis() - lastUpdate) >= INTERVAL_UPDATE*1000) {
        post();
        lastUpdate = millis();
    }

    // if ((millis()-last)/1000 > 10) {
    //     last = millis();

    //     float tdsval = gettds();
    //     // createDocfb(tdsval, 6145, 542, stime(now));
    //     // cnt++;
    // }

    int currentHour = now.hour();
    int currentMinute = now.minute();

    for (int i = 0; i < lenj; i++) {
        bool selisihMenit = currentMinute >= getMint(jams[i]) && (currentMinute - getMint(jams[i])) < TOL;
        if (currentHour == getHour(jams[i]) && selisihMenit && !isPumpOn) {
            isPumpOn = true;
            pumpStartTime = millis();
            pinMode(REL, OUTPUT);
            Serial.println("Pump ON");
            break; // Exit loop after turning on the pump
        }
    }

    // Check if the pump needs to be turned off
    if (isPumpOn && (millis() - pumpStartTime >= interval*1000)) {
        isPumpOn = false;
        pinMode(REL, INPUT);
        Serial.println("Pump OFF");
    }
}
