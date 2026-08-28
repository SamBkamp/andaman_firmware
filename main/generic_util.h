#ifndef GENERIC_UTIL_H
#define GENERIC_UTIL_H

//the random assortment bin of helper functions that don't go anywhere else
//this is bad code orginisation

#include "prot.h"
#include <stdint.h>

void update_sys_time(void);
void print_time(void);
uint8_t wake_driver();
uint8_t sleep_driver();
void init_gpio_pins();
esp_err_t load_data_from_nvs(doser_schedule *sched, step_struct *pump_step_data, uint8_t *hardware_states);

#endif
