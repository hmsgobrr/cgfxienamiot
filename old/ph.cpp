// #include "ph.h"

// #include <Arduino.h>
// #include <ph4502c_sensor.h>

// /* Pinout: https://cdn.awsli.com.br/969/969921/arquivos/ph-sensor-ph-4502c.pdf */
// #define PH4502C_TEMPERATURE_PIN 34
// #define PH4502C_PH_PIN 26
// #define PH4502C_PH_TRIGGER_PIN 35S
// #define PH4502C_CALIBRATION 14.8f
// #define PH4502C_READING_INTERVAL 100
// #define PH4502C_READING_COUNT 10
// // NOTE: The ESP32 ADC has a 12-bit resolution (while most arduinos have 10-bit)
// #define ADC_RESOLUTION 4096.0f

// // Create an instance of the PH4502C_Sensor
// PH4502C_Sensor ph4502c(
//   PH4502C_PH_PIN,
//   PH4502C_TEMPERATURE_PIN,
//   PH4502C_CALIBRATION,
//   PH4502C_READING_INTERVAL,
//   PH4502C_READING_COUNT,
//   ADC_RESOLUTION
// );

// void setupPH() {
//     ph4502c.init();
// }

// float getPH() {
//     return ph4502c.read_ph_level();
// }

// float getTMP() {
//     return ph4502c.read_temp();
// }
