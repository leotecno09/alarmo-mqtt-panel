#include <Arduino.h>

#include "screens.h"
#include "display.h"
#include "mqtt.h"
#include "eeprom_memory.h"

static const ScreenHooks* hooks = nullptr;

static Screen screen = SCR_HOME;
static bool dirty = true;       // flag per indicare se il display va ripulito
static AlarmState alarmState = AS_UNKNOWN;

static unsigned long lastKeyAt = 0;             // ultimo tasto premuto
static unsigned long screenSince = 0;           // tempo passato sull'ultima schermata

static String enteredCode = "";
static const uint8_t MAX_CODE_LEN = 4;

static uint8_t menuCursor = 0;
static uint8_t homecontrolCursor = 0;

static uint8_t standbyMinutes = 5;
static uint8_t standbyEdit = 5;
static const uint8_t STANDBY_MIN = 1;
static const uint8_t STANDBY_MAX = 30;

static const unsigned long INACTIVITY_MS = 15000;
static const unsigned long WAITING_MS = 5000;
static const unsigned long ERROR_MS = 3000;
static const unsigned long SIRENTEST_MS = 3000;

// -- UTILS -- //

static void set_screen(Screen s) {
    screen = s;
    dirty = true;
    screenSince = millis();
}

static void go_home() {
    enteredCode = "";
    set_screen(SCR_HOME);
}

// -- MENU -- //

static void action_siren_test() {
    if (hooks && hooks->siren_test) hooks->siren_test(true);
    set_screen(SCR_SIRENTEST);
}

static void action_standby() {
    standbyEdit = standbyMinutes;
    set_screen(SCR_STANDBY);
}

static void action_goto_housecontrol() {
    set_screen(SCR_HOMECONTROL);
}

struct MenuItem {
    const char* label;
    void (*action)();
};

struct HomeControlItem {
    const char* device_id;
    const char* label;
};

static const MenuItem MENU[] = {
    { "Prova Sirena", action_siren_test },
    { "Standby display", action_standby },
    { "Controllo casa", action_goto_housecontrol }
};

static const HomeControlItem HOMECONTROL_ITEMS[] = {
    { "light.lampadario_leo", "Lamp. Leo" },
    { "light.lampada_divano", "Lamp. Divano" }
};

static const uint8_t MENU_COUNT = sizeof(MENU) / sizeof(MENU[0]);
static const uint8_t HOMECONTROL_COUNT = sizeof(HOMECONTROL_ITEMS) / sizeof(HOMECONTROL_ITEMS[0]);

// ------ BUTTONS MANAGEMENT FOR SCREEN ------- //
static void submit_code() {
    const char* cmd = nullptr;

    switch (alarmState) {
        case AS_DISARMED:
            cmd = "ARM_AWAY";
            break;
        case AS_ARMING:
        case AS_ARMED_AWAY:
        case AS_ARMED_HOME:
        case AS_PENDING:
        case AS_TRIGGERED:
            cmd = "DISARM";
            break;

        default:
            break;
    }

    if (!cmd) return;

    mqtt_alarm_command(cmd, enteredCode.c_str());
    enteredCode = "";

    if (hooks && hooks->code_sent) hooks->code_sent();
    set_screen(SCR_WAITING);
}

// GESTIONE TASTI KEYPAD IN SCHERMATA PIN
static void pin_key(char key) {
    if (key >= '0' && key <= '9') {
        if (enteredCode.length() < MAX_CODE_LEN) {
            enteredCode += key;
            dirty = true;
        }
    } else if (key == '*') {
        if (enteredCode.length() > 0) {
            enteredCode.remove(enteredCode.length() - 1);
            dirty = true;
        } else {
            go_home();
        }
    } else if (key == '#') {
        if (enteredCode.length() > 0) submit_code();
    }
}

// GESTIONE DEI TASTI DEL KEYPAD NELLA SCHERMATA HOME
static void home_key(char key) {
    if (key >= '0' && key <= '9') {
        enteredCode = "";
        set_screen(SCR_PIN);
        pin_key(key);
    } else if (key == '*') {
        // entra nel menu SOLO se non sei in triggered o pending
        if (alarmState == AS_PENDING || alarmState == AS_TRIGGERED || alarmState == AS_ARMING) return;
        menuCursor = 0;
        set_screen(SCR_MENU);
    }
}

// GESTIONE TASTI KEYPAD NEL MENU
static void menu_key(char key) {
    if (key == '2' && menuCursor > 0) {
        menuCursor--;
        dirty = true;
    } else if (key == '8' && menuCursor < MENU_COUNT - 1) {
        menuCursor++;
        dirty = true;
    } else if (key == '#') {
        MENU[menuCursor].action();
    } else if (key == '*') {
        go_home();
    }
}

// GESTIONE TASTI KEYPAD NELL'EDIT STANDBY
static void standby_key(char key) {
    if (key == '2' && standbyEdit < STANDBY_MAX) {
        standbyEdit++;
        dirty = true;
    } else if (key == '8' && standbyEdit > STANDBY_MIN) {
        standbyEdit--;
        dirty = true;
    } else if (key == '#') {
        standbyMinutes = standbyEdit;
        write_setting(standbyMinutes, ADDR_STANDBY);
        set_screen(SCR_MENU);
    } else if (key == '*') {
        set_screen(SCR_MENU);
    }
}

static void homecontrol_key(char key) {
    if (key == '2' && homecontrolCursor > 0) {
        homecontrolCursor--;
        dirty = true;
    } else if (key == '8' && homecontrolCursor < HOMECONTROL_COUNT - 1) {
        homecontrolCursor++;
        dirty = true;
    } else if (key == '#') {
        mqtt_publish("tastierino/toggle", HOMECONTROL_ITEMS[homecontrolCursor].device_id);
    } else if (key == '*') {
        set_screen(SCR_MENU);
    }
}

static void end_siren_test() {
    if (hooks && hooks->siren_test) hooks->siren_test(false);
    set_screen(SCR_MENU);
}

// ----- RENDER ----- //
static void render_home() {
    switch (alarmState) {
        case AS_DISARMED:
            display_clr();
            display_print_centered("SISTEMA", 0);
            display_print_centered("DISARMATO", 1);
            break;
        //display_scroll_txt("Benvenuti nel pannello di gestione di Alarmo. Immettere il codice operativo", 0, 300);
    
        case AS_ARMING:
            //display_clr();
            display_print_centered("INSERIMENTO...", 0);
            break;

        case AS_ARMED_AWAY:
            //display_clr();
            display_move_cur(4, 0);
            display_write_icon(0);
            display_print_str(" ARMATO");
            break;

        case AS_ARMED_HOME:
            //display_clr();
            display_move_cur(1, 0);
            display_write_icon(0);
            display_print_str(" ARMATO CASA");
            break;

        case AS_PENDING:
            display_print_centered("! ATTENZIONE !", 0);
            break;

        case AS_TRIGGERED:
            display_print_centered("! INTRUSIONE !", 0);
            break;

        default:
            break;
    }
}

static void render_pin() {
    display_append(0, 0, "PIN:");
    display_move_cur(0, 1);
    for (uint8_t i = 0; i < enteredCode.length(); i++) display_print_str("*");
}

static void render_menu() {
    char line[17];

    snprintf(line, sizeof(line), ">%-15s", MENU[menuCursor].label);
    display_append(0, 0, line);

    if (menuCursor + 1 < MENU_COUNT) {
        snprintf(line, sizeof(line), " %-15s", MENU[menuCursor + 1].label);
        display_append(0, 1, line);
    }
}

static void render_standby() {
    char line[17];

    display_append(0, 0, "Standby display");
    snprintf(line, sizeof(line), "%2u min   2+ 8-", standbyEdit);
    display_append(0, 1, line);
}

static void render_homecontrol() {
    char line[17];

    snprintf(line, sizeof(line), ">%-15s", HOMECONTROL_ITEMS[homecontrolCursor].label);
    display_append(0, 0, line);

    if (homecontrolCursor + 1 < HOMECONTROL_COUNT) {
        snprintf(line, sizeof(line), " %-15s", HOMECONTROL_ITEMS[homecontrolCursor + 1].label);
        display_append(0, 1, line);
    }
}

static void render() {
    if (!dirty) return;

    dirty = false;

    display_clr();

    switch (screen) {
        case SCR_HOME:
            render_home();
            break;

        case SCR_PIN:
            render_pin();
            break;

        case SCR_WAITING:
            display_print_centered("ATTENDERE", 0);
            display_print_centered("Invio codice...", 1);
            break;

        case SCR_ERROR:
            display_print_centered("CODICE", 0);
            display_print_centered("ERRATO", 1);
            break;

        case SCR_MENU:
            render_menu();
            break;

        case SCR_STANDBY:
            render_standby();
            break;

        case SCR_SIRENTEST:
            display_print_centered("TEST SIRENA", 0);
            display_print_centered("Premi un tasto", 1);
            break;

        case SCR_HOMECONTROL:
            render_homecontrol();
            break;
    }
}

// ---- INTERFACCIA PUBBLICA ---- //
void screens_init(const ScreenHooks* h) {
    hooks = h;
    read_setting(&standbyMinutes, ADDR_STANDBY);
    lastKeyAt = millis();
    set_screen(SCR_HOME);
}

void screens_on_key(char key) {
    lastKeyAt = millis();

    switch (screen) {
        case SCR_HOME:  
            home_key(key);
            break;

        case SCR_PIN:
            pin_key(key);
            break;

        case SCR_MENU:
            menu_key(key);
            break;

        case SCR_STANDBY:
            standby_key(key);
            break;

        case SCR_HOMECONTROL:
            homecontrol_key(key);
            break;

        case SCR_SIRENTEST:
            end_siren_test();
            break;

        default:
            break;
    }
}

void screens_on_alarm_state(AlarmState s) {
    if (s == alarmState) return;
    alarmState = s;

    // eventi importanti
    if (s == AS_PENDING || s == AS_TRIGGERED) {
        go_home();
        return;
    }

    if (screen == SCR_HOME || screen == SCR_WAITING || screen == SCR_ERROR) {
        go_home();
    }
}

bool screens_on_invalid_code() {
    if (screen != SCR_WAITING) return false;
    set_screen(SCR_ERROR);
    return true;
}

uint8_t screens_standby_minutes() {
    return standbyMinutes;
}

void screens_update() {
    unsigned long now = millis();

    switch (screen) {
        case SCR_PIN:
        case SCR_MENU:
        case SCR_STANDBY:
            if (now - lastKeyAt >= INACTIVITY_MS) go_home();
            break;

        case SCR_WAITING:
            if (now - screenSince >= WAITING_MS) go_home();
            break;

        case SCR_ERROR:
            if (now - screenSince >= ERROR_MS) go_home();
            break;

        case SCR_SIRENTEST:
            if (now - screenSince >= SIRENTEST_MS) end_siren_test();
            break;

        default:
            break;
    }

    render();

    if (screen == SCR_HOME && alarmState == AS_UNKNOWN) {
        display_scroll_txt("Benvenuti nel pannello di gestione di Alarmo. In attesa di uno stato...", 0, 300);
    }
}