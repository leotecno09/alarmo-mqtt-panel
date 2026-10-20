#include <EEPROM.h>
#include "eeprom_memory.h"

static const int EEPROM_SIZE = ADDR_COUNT;
static const uint8_t MAGIC = 0xA5;      // indica che sono dati scritti da "noi"

void read_setting(uint8_t* target, Address dest_addr) {
    EEPROM.begin(EEPROM_SIZE);

    if (EEPROM.read(ADDR_MAGIC) == MAGIC) {
        uint8_t v = EEPROM.read(dest_addr);
        *target = v;
    }
}

void write_setting(uint8_t value, Address dest_addr) {
    if (EEPROM.read(ADDR_MAGIC) == MAGIC && EEPROM.read(dest_addr) == value) return;

    EEPROM.write(ADDR_MAGIC, MAGIC);
    EEPROM.write(dest_addr, value);
    EEPROM.commit();
}