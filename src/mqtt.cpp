#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "display.h"
#include "panel.h"

const char* WIFI_SSID = "xxx";
const char* WIFI_PASSWORD = "xxx";

const char* MQTT_SERVER = "10.24.30.139";
const int MQTT_PORT = 1883;
const char* MQTT_USER = "xxx";
const char* MQTT_PASSWORD = "xxx";

const char* ALARMO_COMMANDS_TOPIC = "alarmo/command";
const char* ALARMO_STATE_TOPIC = "alarmo/state";
const char* ALARMO_EVENT_TOPIC = "alarmo/event";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

static void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    Serial.println("Chiamata callback");

    if (strcmp(topic, ALARMO_STATE_TOPIC) == 0) {
        char state[32];

        unsigned int copyLength = length;

        if (copyLength >= sizeof(state))
            copyLength = sizeof(state) - 1; // tronca l'input per non andare oltre al buffer
        
        memcpy(state, payload, copyLength);
        state[copyLength] = '\0';

        Serial.print("[MQTT] Alarmo state: ");
        Serial.println(state);

        panel_set_alarm_state(state);
    }

    else if (strcmp(topic, ALARMO_EVENT_TOPIC) == 0) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload, length);

        if (err) {
            Serial.print("[MQTT] Error JSON parsing: ");
            Serial.println(err.c_str());
            return;
        }

        const char* event = doc["event"];

        panel_handle_event(event);
    }
}

void connect_wifi() {
    display_print("WIFI", "MQTT");
    display_append(10, 0, "NO");
    display_append(10, 1, "NO");

    Serial.println("[WiFi] Connecting to WiFi...");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] WiFi connected!");
        Serial.println(WiFi.localIP());

        display_append(10, 0, "OK");
    } else {
        Serial.println("[WiFi] WiFi error. Not connected");
        Serial.println(WiFi.status());

        display_append(10, 0, "ERR");        
    }

}

void connect_mqtt(bool isReconnecting) {
    if (isReconnecting) 
        display_print("Riconnessione MQTT", "Stato:");

    while (!mqttClient.connected()) {
        Serial.print("[MQTT] Connecting to broker...");
        
        if (mqttClient.connect("alarm_panel", MQTT_USER, MQTT_PASSWORD) && mqttClient.subscribe(ALARMO_STATE_TOPIC) && mqttClient.subscribe(ALARMO_EVENT_TOPIC)) {
            Serial.println("OK");

            display_append(10, 1, "OK");

            mqttClient.publish("alarm/panel/status", "online", true);
        } else {
            Serial.print("error: ");
            Serial.println(mqttClient.state());

            display_append(10, 1, "ERR");

            delay(2000);        // male usare delay()
        }
    }
}

void mqtt_init() {
    connect_wifi();
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqtt_callback);
    mqttClient.setBufferSize(512);
    connect_mqtt(false);
}

void mqtt_loop() {
    if (!mqttClient.connected()) {
        connect_mqtt(true);
    }

    mqttClient.loop();
}

bool mqtt_is_connected() {
    return mqttClient.connected();
}

void mqtt_alarm_command(const char* cmd, const char* code) {
    if (!mqttClient.connected()) {
        Serial.println("[MQTT] Not connected!");
        return;
    }

    char payload[128];

    snprintf(payload, sizeof(payload), "{\"command\":\"%s\",\"code\":\"%s\"}", cmd, code);

    Serial.println("[MQTT] Sending data to Alarmo...");
    
    mqttClient.publish(ALARMO_COMMANDS_TOPIC, payload, false);
}

void mqtt_publish(const char* topic, const char* payload) {
    if (!mqttClient.connected()) return;

    mqttClient.publish(topic, payload);
}