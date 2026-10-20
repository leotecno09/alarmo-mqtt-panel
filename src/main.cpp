#include <Arduino.h>
#include <ArduinoOTA.h>

#include "display.h"
#include "input.h"
#include "mqtt.h"
#include "panel.h"

// void ledSequence() {
//   digitalWrite(LED_R, HIGH);
//   digitalWrite(LED_G, HIGH);
//   digitalWrite(LED_B, HIGH);
//   delay(2000);

//   digitalWrite(LED_R, LOW);
//   digitalWrite(LED_G, LOW);
//   digitalWrite(LED_B, LOW);
//   delay(2000);
  
//   digitalWrite(LED_R, HIGH);
//   delay(100);
//   digitalWrite(LED_G, HIGH);
//   delay(100);
//   digitalWrite(LED_B, HIGH);
//   delay(2000);

//   digitalWrite(LED_B, LOW);
//   delay(100);
//   digitalWrite(LED_G, LOW);
//   delay(100);
//   digitalWrite(LED_R, LOW);

//   delay(2000);
// }

void setup() {
  Serial.begin(115200);

  display_init();
  delay(1000);    // male delay()
  mqtt_init();

  while (!mqtt_is_connected()) {}

  // initalize OTA
  ArduinoOTA.setHostname("tastierino");
  ArduinoOTA.setPassword("xxxx");
  ArduinoOTA.begin();

  panel_init();

  Serial.println("SYSTEM ALIVE");
}

void loop() {
  mqtt_loop();

  panel_update();
  siren_update();

  ArduinoOTA.handle();
}