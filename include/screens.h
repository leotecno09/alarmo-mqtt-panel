#ifndef _SCREENS_H
#define _SCREENS_H

#include <Arduino.h>

enum AlarmState { AS_UNKNOWN, AS_DISARMED, AS_ARMING, AS_ARMED_AWAY, AS_ARMED_HOME, AS_PENDING, AS_TRIGGERED };
enum Screen { SCR_HOME, SCR_PIN, SCR_WAITING, SCR_ERROR, SCR_MENU, SCR_STANDBY, SCR_SIRENTEST, SCR_HOMECONTROL };

struct ScreenHooks {
    void (*siren_test)(bool on);
    void (*code_sent)();
};

void screens_init(const ScreenHooks* hooks);
void screens_update();
void screens_on_key(char key);
void screens_on_alarm_state(AlarmState s);
bool screens_on_invalid_code();
uint8_t screens_standby_minutes();

#endif