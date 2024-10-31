#include <Arduino.h>

#include "RTClib.h"

#include "firebase.h"
#include "tds.h"

unsigned long last = millis();
int cnt = 1;

RTC_DS3231 rtc;

void setup() {
    Serial.begin(115200);
    initfb();

    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        while (1) delay(300);
    }

    if (rtc.lostPower()) {
        Serial.println("RTC lost power, let's set the time!");
        // When time needs to be set on a new device, or after a power loss, the
        // following line sets the RTC to the date & time this sketch was compiled
        //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        // This line sets the RTC with an explicit date & time, for example to set
        // January 21, 2014 at 3am you would call:
        // rtc.adjust(DateTime(2024, 10, 27, 12, 44, 0));
    }

    // When time needs to be re-set on a previously configured device, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    //rtc.adjust(DateTime(2024, 10, 27, 12, 44, 0));
}

void loop() {
    loopfb();
    looptds();

    DateTime now = rtc.now();

    if ((millis()-last)/1000 > 10) {
        last = millis();

        float tdsval = gettds();
        createDocfb(tdsval, 6145, 542, cnt);
        cnt++;
    }
}
