#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "display.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);


void display_init() {
    Wire.begin();
    lcd.init();

    lcd.backlight();

    Serial.println("[LCD] Initializing LCD...");

    // boot splash
    lcd.setCursor(5, 0);
    lcd.print("ALARMO");

    lcd.setCursor(4, 1);
    lcd.print("Booting...");
    
    lcd.createChar(0, lockIcon);
}

void display_clr() {
    lcd.clear();
}

void display_clr_line(uint8_t row) {
    lcd.setCursor(0, row);
    lcd.print("                ");
}

void display_print(const char* line1, const char* line2) {
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print(line1);

    lcd.setCursor(0, 1);
    lcd.print(line2);
}

void display_write_icon(uint8_t slot) {
    lcd.write(byte(slot));
}

void display_print_centered(const String &text, uint8_t row) {
    display_clr_line(row);

    int spaces = (16 - text.length()) / 2;

    if (spaces < 0) spaces = 0;

    lcd.setCursor(spaces, row);
    lcd.print(text);
}

void display_move_cur(uint8_t x, uint8_t y) {
    lcd.setCursor(x, y);
}

void display_print_str(const char* str) {
    lcd.print(str);
}

void display_append(uint8_t x, uint8_t y, const char* str) {
    lcd.setCursor(x, y);
    lcd.print(str);
}

void display_scroll_txt(const String& text, uint8_t row, uint16_t interval) {
    static String latestText;
    static unsigned int pos = 0;
    static unsigned long latest = 0;

    if (text != latestText) {
        latestText = text;
        pos = 0;
        latest = millis() - interval;
    }

    unsigned long now = millis();

    if (now - latest < interval) return;
    latest = now;

    unsigned int len = latestText.length();

    lcd.setCursor(0, row);
    for (uint8_t i = 0; i < 16; i++) {
        unsigned int p = pos + i;
        char c = ' ';
        if (p >= 16 && p < 16 + len) c = latestText[p - 16];
        lcd.write(c);
    }

    pos++;
    if (pos > 16 + len) pos = 0;
}

void lcd_standby() {
    lcd.noBacklight();
    lcd.noDisplay();
}

void lcd_resume() {
    lcd.backlight();
    lcd.display();
}