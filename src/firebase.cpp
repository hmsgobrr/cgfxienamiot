#include "firebase.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>

#include <FirebaseClient.h>

#define WIFI_SSID "IkanNetwork"
#define WIFI_PASSWORD "hammaslolol"

#define API_KEY "AIzaSyAKUhLsZWHKe4kO-cDFp-innAMwBAbBGLQ"
#define USER_EMAIL "metalcorehms@gmail.com"
#define USER_PASSWORD "hisyamganteng"
#define FIREBASE_PROJECT_ID "cgfxienam"

void asyncCB(AsyncResult &aResult);
void printResult(AsyncResult &aResult);
String getTimestampString(uint64_t sec, uint32_t nano);

using AsyncClient = AsyncClientClass;

DefaultNetwork network;
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);
FirebaseApp app;
WiFiClientSecure ssl_client;
AsyncClient aClient(ssl_client, getNetwork(network));

Firestore::Documents Docs;

void initfb() {
     WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();

    Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

    Serial.println("Initializing app...");

    ssl_client.setInsecure();

    initializeApp(aClient, app, getAuth(user_auth), asyncCB, "authTask");

    // Binding the FirebaseApp for authentication handler.
    // To unbind, use Docs.resetApp();
    app.getApp<Firestore::Documents>(Docs);
}
void loopfb() {
    app.loop();
    Docs.loop();
}

void createDocfb(double tds, double ph, double temp, int dc) {
    String documentPath = "hisyamganteng/b";
    documentPath += dc;

    Values::DoubleValue tdsv(number_t(tds, 3));
    Values::DoubleValue phv(number_t(ph, 3));
    Values::DoubleValue tempv(number_t(temp, 3));

    // Values::TimestampValue tsV(getTimestampString(1712674441, 999999999));

    Document<Values::Value> doc("tds", Values::Value(tdsv));
    doc.add("ph", Values::Value(phv));
    doc.add("tempv", Values::Value(tempv));


    Serial.println("Create document... ");

    Docs.createDocument(aClient, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, DocumentMask(), doc, asyncCB, "createDocumentTask");
}

void asyncCB(AsyncResult &aResult)
{
    // WARNING!
    // Do not put your codes inside the callback and printResult.

    printResult(aResult);
}

void printResult(AsyncResult &aResult)
{
    if (aResult.isEvent())
    {
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
    }

    if (aResult.isDebug())
    {
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError())
    {
        Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }

    if (aResult.available())
    {
        Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
    }
}

String getTimestampString(uint64_t sec, uint32_t nano)
{
    if (sec > 0x3afff4417f)
        sec = 0x3afff4417f;

    if (nano > 0x3b9ac9ff)
        nano = 0x3b9ac9ff;

    time_t now;
    struct tm ts;
    char buf[80];
    now = sec;
    ts = *localtime(&now);

    String format = "%Y-%m-%dT%H:%M:%S";

    if (nano > 0)
    {
        String fraction = String(double(nano) / 1000000000.0f, 9);
        fraction.remove(0, 1);
        format += fraction;
    }
    format += "Z";

    strftime(buf, sizeof(buf), format.c_str(), &ts);
    return buf;
}
