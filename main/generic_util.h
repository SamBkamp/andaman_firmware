#ifndef GENERIC_UTIL_H
#define GENERIC_UTIL_H

#include <stdint.h>

void update_sys_time(void);
void print_time(void);
uint8_t wake_driver();
uint8_t sleep_driver();
void init_gpio_pins();

#endif
