#include <Arduino.h>
#include <strings.h>

#include "display.h"
#include "panel.h"
#include "mqtt.h"
#include "screens.h"

// -- STANDBY -- //
static uint32_t lastActivity = 0;
static bool sleeping = false;
static bool ledsSuspended = false;

// -- STATO ALLARME -- //
static AlarmState currentState = AS_UNKNOWN;

// -- SIRENA -- //
static bool isSirenActive = false;
static uint16_t sirenFreq = 700;
static int sirenPhase = 0;
static unsigned long sirenTimer = 0;
static const int BUZZER_PIN = 22;

// -- LED -- //
static const int LED_R = 20;
static const int LED_G = 18;
static const int LED_B = 21;


enum LedColor { LED_NONE, LED_RED, LED_GREEN, LED_BLUE };

static LedColor ledColor = LED_NONE;
static bool ledBlinking = false;
static bool ledOn = false;
static unsigned long ledTimer = 0;
static uint8_t ledBlinkCount = 0;

static const unsigned long LED_BLINK_INTERVAL = 300;
static const uint8_t LED_BLINK_TOTAL = 2;

static void led_write(LedColor color, bool on) {
    if (ledsSuspended) {
        digitalWrite(LED_R, LOW);
        digitalWrite(LED_G, LOW);
        digitalWrite(LED_B, LOW);
        return;
    }

    digitalWrite(LED_R, (color == LED_RED   && on) ? HIGH : LOW);
    digitalWrite(LED_G, (color == LED_GREEN && on) ? HIGH : LOW);
    digitalWrite(LED_B, (color == LED_BLUE  && on) ? HIGH : LOW);
}

static void led_suspend() {
    ledsSuspended = true;
    led_write(LED_NONE, false);
}

static void led_resume() {
    ledsSuspended = false;
    led_write(ledColor, ledOn);
}

static void led_off() {
    ledColor = LED_NONE;
    ledBlinking = false;
    led_write(LED_NONE, false);
}

static void led_set_solid(LedColor color) {
    ledColor = color;
    ledBlinking = false;
    ledOn = true;
    led_write(color, true);
}

static void led_start_blink(LedColor color) {
    ledColor = color;
    ledBlinking = true;
    ledOn = true;
    ledBlinkCount = 0;
    ledTimer = millis();
    led_write(color, true);
}

void led_update() {
    if (!ledBlinking) return;
 
    unsigned long now = millis();
    if (now - ledTimer >= LED_BLINK_INTERVAL) {
        ledTimer = now;
        ledOn = !ledOn;
        led_write(ledColor, ledOn);
 
        if (ledOn) {
            ledBlinkCount++;
            if (ledBlinkCount >= LED_BLINK_TOTAL) {
                ledBlinking = false;
                led_write(ledColor, true); // termina con il colore fisso acceso
            }
        }
    }
}

// --------------------------- //

// -- KEYPAD -- //

const byte ROWS = 4;
const byte COLS = 3;

char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

byte rowPins[ROWS] = {16, 14, 10, 7};
byte colPins[COLS] = {11, 17, 8};

//Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
static char scan_keypad() {
    char found = 0;
    uint8_t count = 0;

    for (uint8_t c = 0; c < COLS; c++) {
        pinMode(colPins[c], OUTPUT);
        digitalWrite(colPins[c], LOW);
        delayMicroseconds(200);              // tempo di assestamento

        for (uint8_t r = 0; r < ROWS; r++) {
            if (digitalRead(rowPins[r]) == LOW) {
                found = keys[r][c];
                count++;
            }
        }

        pinMode(colPins[c], INPUT);          // colonna in alta impedenza
        delayMicroseconds(200);              // lascia risalire le righe
    }

    return (count == 1) ? found : 0;         // più tasti insieme = ignora
}

static void keypad_poll() {
    static char lastRaw = 0, stable = 0;
    static unsigned long changedAt = 0;

    char raw = scan_keypad();
    unsigned long now = millis();

    if (raw != lastRaw) {
        lastRaw = raw;
        changedAt = now;
    } else if (now - changedAt >= 25 && raw != stable) {
        stable = raw;
        if (raw) panel_handle_key(raw);      // solo sul fronte di pressione
    }
}

// --------------------------- // 

static void wake_display() {
    lastActivity = millis();
    if (sleeping) {
        lcd_resume();
        led_resume();
        sleeping = false;
    }
}

// -- BUZZER SIRENA -- //

static void beep() {
    tone(BUZZER_PIN, 329.63, 100);
}

static void start_siren() {
    isSirenActive = true;
    sirenPhase = 0;
    sirenTimer = millis();

    tone(BUZZER_PIN, sirenFreq);
}

static void stop_siren() {
    isSirenActive = false;
    noTone(BUZZER_PIN);
}

void siren_update() {
    if (!isSirenActive) return;

    unsigned long now = millis();

    if (now - sirenTimer >= 400) {
        sirenTimer = now;

        if (sirenPhase == 0) {
            tone(BUZZER_PIN, 700);
            sirenPhase = 1;
        } else {
            tone(BUZZER_PIN, 1100);
            sirenPhase = 0;
        }
    }
}

static void hook_siren_test(bool on) {
    if (on) start_siren();
    else    stop_siren();
}

static void hook_code_sent() {
    led_start_blink(LED_BLUE);
}

static const ScreenHooks screenHooks = {
    hook_siren_test,
    hook_code_sent
};

// conversione stato alarmo in enum
static AlarmState parse_state(const char* s) {
    if (strcmp(s, "disarmed") == 0) return AS_DISARMED;
    if (strcmp(s, "arming") == 0) return AS_ARMING;
    if (strcmp(s, "armed_away") == 0) return AS_ARMED_AWAY;
    if (strcmp(s, "armed_home") == 0) return AS_ARMED_HOME;
    if (strcmp(s, "pending") == 0) return AS_PENDING;
    if (strcmp(s, "triggered") == 0) return AS_TRIGGERED;

    return AS_UNKNOWN;
}

// -- API PUBBLICHE -- //

void panel_init() {
    pinMode(BUZZER_PIN, OUTPUT);

    pinMode(LED_R, OUTPUT);
    pinMode(LED_G, OUTPUT);
    pinMode(LED_B, OUTPUT);
    led_off();

    //keypad.addEventListener(keypad_event_handler);
    //keypad.setDebounceTime(20);

    for (uint8_t r = 0; r < ROWS; r++) pinMode(rowPins[r], INPUT_PULLUP);

    // init tone
    tone(BUZZER_PIN, 415.30, 300);
    tone(BUZZER_PIN, 830.61, 500);

    screens_init(&screenHooks);
}

void panel_handle_key(char key) {
    if (!isSirenActive) beep();

    bool wasSleeping = sleeping;
    wake_display();
    if (wasSleeping) return;

    screens_on_key(key);
}

void panel_handle_event(const char* event) {
    if (event == nullptr) return;

    if (strcmp(event, "INVALID_CODE_PROVIDED") == 0) {
        if (screens_on_invalid_code()) {
            led_start_blink(LED_RED);
        }
    }
}

void panel_update() {
    //keypad.getKey();
    keypad_poll();
    led_update();
    screens_update();

    if (currentState == AS_TRIGGERED || currentState == AS_PENDING || currentState == AS_ARMING) {
        lastActivity = millis();
    }

    uint32_t timeoutMs = (uint32_t)screens_standby_minutes() * 60UL * 1000UL;

    if (!sleeping && millis() - lastActivity > timeoutMs) {
        lcd_standby();
        led_suspend();
        sleeping = true;
    }
}

void panel_set_alarm_state(const char* state) {
    AlarmState s = parse_state(state);
    bool changed = (s != currentState);

    Serial.print("[PANEL] Received state from Alarmo: ");
    Serial.print(state);
    Serial.println(changed ? " (changed)" : " (same)");

    if (!changed) return;
    currentState = s;

    wake_display();

    if (s == AS_TRIGGERED)
        start_siren();
    else
        stop_siren();

    switch (s) {
        case AS_DISARMED:
            led_start_blink(LED_RED);
            break;

        case AS_ARMING:
            led_start_blink(LED_BLUE);
            break;

        case AS_ARMED_AWAY:
        case AS_ARMED_HOME:
            led_start_blink(LED_GREEN);
            break;

        case AS_PENDING:
            led_start_blink(LED_BLUE);
            break;
        
        case AS_TRIGGERED:
            led_start_blink(LED_RED);
            break;

        default: break;
    }

    screens_on_alarm_state(s);
}