#ifndef PANEL_H
#define PANEL_H

void panel_init();
void panel_handle_key(char);
void panel_update();
void panel_set_alarm_state(const char*);
void panel_handle_event(const char*);
void siren_update();

#endif