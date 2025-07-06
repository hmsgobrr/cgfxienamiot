 // Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include <SPI.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "RTClib.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define TdsSensorPin 33
const int relay = 5;
#define VREF 3.3              // analog reference voltage(Volt) of the ADC
#define SCOUNT  30            // sum of sample point

int analogBuffer[SCOUNT];     // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;

float averageVoltage = 0;
float tdsValue = 0;
float temperature = 25;       // current temperature for compensation

bool mindelay = false;
// const char* ssid = "AULA ALCENT";
// const char* password = "aula48alcent";
const char* ssid = "RUANG 303";
const char* password = "alfadp303uu";
String time_str;

const int MAXJAM = 50;
char jams[7][MAXJAM];
int jamslen;
int interval = 10;

const int TOL = 10;         // Toleransi keterlambatan menyiram (karena perangkat mati atau hal lain), dalam menit
const int INTER_POST = 40;   // Interval POST data ph, suhu, dan tds, dalam detik (MINIMAL 4.32 detik)
const int INTER_GET = 15;    // Interval GET data setting jam-jam, dan durasi penyiraman, dalam detik (MINIMAL 3.46 detik)

HTTPClient http;
RTC_DS3231 rtc;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

//ph

#include <ph4502c_sensor.h>

/* Pinout: https://cdn.awsli.com.br/969/969921/arquivos/ph-sensor-ph-4502c.pdf */
#define PH4502C_TEMPERATURE_PIN 34
#define PH4502C_PH_PIN 32
#define PH4502C_PH_TRIGGER_PIN 35S
#define PH4502C_CALIBRATION 14.8f
#define PH4502C_READING_INTERVAL 100
#define PH4502C_READING_COUNT 10
// NOTE: The ESP32 ADC has a 12-bit resolution (while most arduinos have 10-bit)
#define ADC_RESOLUTION 4096.0f

// Create an instance of the PH4502C_Sensor
PH4502C_Sensor ph4502c(
  PH4502C_PH_PIN,
  PH4502C_TEMPERATURE_PIN,
  PH4502C_CALIBRATION,
  PH4502C_READING_INTERVAL,
  PH4502C_READING_COUNT,
  ADC_RESOLUTION
);

// median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen){
  int bTab[iFilterLen];
  for (byte i = 0; i<iFilterLen; i++)
  bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0){
    bTemp = bTab[(iFilterLen - 1) / 2];
  }
  else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}

void get_data(){
    if(WiFi.status()== WL_CONNECTED) {
        unsigned long startget = millis();
        String serverPath = "https://firestore.googleapis.com/v1/projects/cgfxienam/databases/(default)/documents/setting/timer";
        http.begin(serverPath.c_str());

        // If you need Node-RED/server authentication, insert user and password below
        //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

        // Send HTTP GET request
        int httpResponseCode = http.GET();

        if (httpResponseCode>0) {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);

            String payload = http.getString();
            Serial.println(payload);

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload.c_str());
            if (error) {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.f_str());
                return;
            }
            JsonArray values = doc["fields"]["times"]["arrayValue"]["values"].as<JsonArray>();
            memset(jams, 0, sizeof(jams));
            int i = 0;
            for (JsonVariant v : values) {
                const char* strval = v["stringValue"];
                for (int j = 0; j < 5; j++) {   
                    jams[i][j] = strval[j];
                }
                i++;
            }
            jamslen = i;

            for(int j = 0; j < jamslen; j++){
                Serial.print("Times: ");
                Serial.println(jams[j]);
            }
            Serial.print("Delay GET total: ");
            Serial.println(millis()-startget);
        }
        else {
            Serial.print("Error code: ");
            Serial.println(httpResponseCode);
        }
        // Free resources
        http.end();


        serverPath = "https://firestore.googleapis.com/v1/projects/cgfxienam/databases/(default)/documents/setting/interval";
        http.begin(serverPath.c_str());
        
        // If you need Node-RED/server authentication, insert user and password below
        //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
        
        // Send HTTP GET request
        httpResponseCode = http.GET();
        
        if (httpResponseCode>0) {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);

            String payload = http.getString();
            Serial.println(payload);

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload.c_str());
            if (error) {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.f_str());
                return;
            }
            interval = doc["fields"]["interval"]["integerValue"].as<int>();

            Serial.print("Interval: ");
            Serial.println(interval);
        }
        else {
            Serial.print("Error code: ");
            Serial.println(httpResponseCode);
        }
        // Free resources
        http.end();

    }
    else {
        Serial.println("WiFi Disconnected");
    }
}

void post_data(float ph, float tds, float temp, DateTime &now) {
    if (WiFi.status() == WL_CONNECTED) {
        unsigned long startpost = millis();
        String postPath = "https://firestore.googleapis.com/v1/projects/cgfxienam/databases/(default)/documents/letest";
        // Your Domain name with URL path or IP address with path
        http.begin(postPath);

        // If you need Node-RED/server authentication, insert user and password below
        //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

        String time = now.year() + String("/") + now.month() + String("/") + now.day() + String(" ");

        if (now.hour() < 10) time += "0";
        time += now.hour();
        time += ":";
        if (now.minute() < 10) time += "0";
        time += now.minute();

        JsonDocument doc;
        doc["fields"]["ph"]["doubleValue"] = ph;
        doc["fields"]["tds"]["doubleValue"] = tds;
        doc["fields"]["temp"]["doubleValue"] = temp;
        doc["fields"]["time"]["stringValue"] = time.c_str();
        String jeson;
        serializeJson(doc, jeson);

        http.addHeader("Content-Type", "application/json");
        int httpResponseCode = http.POST(jeson);

        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        // Free resources
        http.end();

        Serial.print("Delay POST total: ");
        Serial.println(millis()-startpost);
    } else {
        Serial.println("WiFi Disconnected");
    }
}

int hour(const char* str) {
    String a;
    a += str[0];
    a += str[1];

    return a.toInt();
}
int mint(const char* str) {
    String a;
    a += str[3];
    a += str[4];

    return a.toInt();
}

void setup () {
  Serial.begin(115200);
  pinMode(TdsSensorPin,INPUT);
  ph4502c.init();
  pinMode(relay, INPUT);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

#ifndef ESP8266
  while (!Serial); // wait for serial port to connect. Needed for native USB
#endif

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2024, 10, 27, 12, 44, 0));
  }
  // When time needs to be re-set on a previously configured device, the
  // following line sets the RTC to the date & time this sketch was compiled
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // This line setsthe RTC with an explicit date & time, for example to set
  // January 21, 2014 at 3am you would call:
  //rtc.adjust(DateTime(2024, 10, 27, 12, 44, 0));
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);
}


void loop () {
    DateTime now = rtc.now();
    int jam=now.hour();
    int menit=now.minute();
    int detik=now.second();
    static unsigned long analogSampleTimepoint = millis();
    if(millis()-analogSampleTimepoint > 40U){     //every 40 milliseconds,read the analog value from the ADC
        analogSampleTimepoint = millis();
        analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);    //read the analog value and store into the buffer
        analogBufferIndex++;
        if(analogBufferIndex == SCOUNT){ 
            analogBufferIndex = 0;
        }
    }   

    static unsigned long printTimepoint = millis();
    if(millis()-printTimepoint > 800U){
        printTimepoint = millis();
        for(copyIndex=0; copyIndex<SCOUNT; copyIndex++){
            analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
            
            // read the analog value more stable by the median filtering algorithm, and convert to voltage value
            averageVoltage = getMedianNum(analogBufferTemp,SCOUNT) * (float)VREF / 4096.0;
            
            //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0)); 
            float compensationCoefficient = 1.0+0.02*(temperature-25.0);
            //temperature compensation
            float compensationVoltage=averageVoltage/compensationCoefficient;
            
            //convert voltage value to tds value
            tdsValue=(133.42*compensationVoltage*compensationVoltage*compensationVoltage - 255.86*compensationVoltage*compensationVoltage + 857.39*compensationVoltage)*0.5;
            
            //Serial.print("voltage:");
            //Serial.print(averageVoltage,2);
            //Serial.print("V   ");
            Serial.print("TDS Value:");
            Serial.print(tdsValue,0);
            Serial.println("ppm");
        }
    }

    Serial.println("Water Temperature reading:"
        + String(ph4502c.read_temp()));

    // Read the pH level by average
    Serial.println("pH Level Reading: "
        + String(ph4502c.read_ph_level()));

    // Read a single pH level value
    Serial.println("pH Level Single Reading: "
        + String(ph4502c.read_ph_level_single()));

    temperature = rtc.getTemperature();
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");

    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" (");
    Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(jam, DEC);
    Serial.print(':');
    Serial.print(menit, DEC);
    Serial.print(':');
    Serial.print(detik, DEC);
    Serial.println();

    time_str = "";

    if (jam < 10)
     {time_str += "0";}
    time_str += now.hour();
    time_str += ":";
    if (now.minute() < 10) 
      {time_str += "0";}
    time_str += now.minute();

    static unsigned long siramTimep = 0;
    static bool lagiNyiram = false;
    for (int i = 0; i < jamslen; i++) {
        Serial.print("checking: ");
        Serial.print(jams[i]);
        Serial.print(" with ");
        Serial.println(time_str);
        unsigned int ae = millis() - siramTimep;
        if(!lagiNyiram && jam == hour(jams[i]) && menit == mint(jams[i]) && (siramTimep == 0 || ae > 60000)){
            pinMode(relay, OUTPUT);
            lagiNyiram = true;
            siramTimep = millis();
        }
    }
    Serial.print("SiramTemp ");
    Serial.println(siramTimep);
    Serial.print("LagiNyiram ");
    Serial.println(lagiNyiram);
    Serial.print("MILIS ");
    Serial.println(millis()-siramTimep);
    Serial.print("neles ");
    Serial.println(millis());
    

    if (!lagiNyiram) {
        pinMode(relay, INPUT);
        mindelay = true;
    }

    if (lagiNyiram && millis()-siramTimep >= interval*1000 && mindelay == true) {
    Serial.println("MASUKIF ");

        pinMode(relay, INPUT);
        lagiNyiram = false;
        mindelay = false;
    }

    Serial.println();
    
    static unsigned long getDataTimep = millis();
    if (millis() - getDataTimep > INTER_GET*1000) {
        getDataTimep = millis();
        get_data();
    }

    static unsigned long postDataTimep = millis();
    if (millis() - postDataTimep > INTER_POST*1000) {
        postDataTimep = millis();
        post_data(ph4502c.read_ph_level(), tdsValue, rtc.getTemperature(), now);
    }

    time_str += ":";
    if (now.second() < 10) 
      {time_str += "0";}
    time_str += now.second();

    display.clearDisplay();
    // display temperature
    display.setTextSize(1);
    display.setCursor(0,0);
    display.print("Waktu Jam Digital: ");
    display.setTextSize(2);
    display.setCursor(0,10);
    display.print(time_str);
    // display.print(" ");
    // display.setTextSize(1);
    // display.cp437(true);
    // display.write(167);
    // display.setTextSize(2);
    // display.print("WIB");
    
    // display humidity
    // display.setTextSize(1);
    // display.setCursor(0, 35);
    // display.print("Humidity: ");
    // display.setTextSize(2);
    // display.setCursor(0, 45);
    // display.print(String(bme.readHumidity()));
    // display.print(" %"); 
    
    display.display();
    delay(500);
}
