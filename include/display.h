#ifndef DISPLAY_H
#define DISPLAY_H

static byte lockIcon[8] = {
    0b00100,
    0b01110,
    0b01010,
    0b11111,
    0b11111,
    0b11011,
    0b11111,
    0b00000
};

void display_init();
void display_clr();
void display_clr_line(uint8_t);
void display_print(const char*, const char*);
void display_write_icon(uint8_t);
void display_move_cur(uint8_t, uint8_t);
void display_print_str(const char*);
void display_append(uint8_t, uint8_t, const char*);
void display_print_centered(const String&, uint8_t);
void display_scroll_txt(const String&, uint8_t, uint16_t);
void lcd_standby();
void lcd_resume();

#endif