#ifndef MQTT_H
#define MQTT_H

void connect_wifi();
void connect_mqtt(bool);
void mqtt_init();
void mqtt_loop();
bool mqtt_is_connected();
void mqtt_alarm_command(const char*, const char*);
void mqtt_publish(const char*, const char*);

#endif