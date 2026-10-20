#ifndef _EEPROM_H
#define _EEPROM_H

enum Address {
    ADDR_MAGIC = 0,
    ADDR_STANDBY,
    ADDR_COUNT
};

void read_setting(uint8_t*, Address);
void write_setting(uint8_t, Address);

#endif