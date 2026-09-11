#ifndef GENERIC_UTIL_H
#define GENERIC_UTIL_H

//the random assortment bin of helper functions that don't go anywhere else
//this is bad code orginisation

#include "prot.h"
#include <stdint.h>

typedef esp_err_t (*nvs_load_cb)(void *data);
typedef esp_err_t (*nvs_store_cb)(void *data);

void update_sys_time(void);
void print_time(void);
uint8_t wake_driver();
uint8_t sleep_driver();
void init_gpio_pins();
esp_err_t load_or_default(nvs_load_cb load, nvs_store_cb store, void* data, void* def_val);

#endif
