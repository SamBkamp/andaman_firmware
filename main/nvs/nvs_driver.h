#ifndef NVS_DRIVER_H
#define NVS_DRIVER_H

#include "prot.h"
#include <stdint.h>

esp_err_t load_schedule(void *schedule);
esp_err_t store_sched(void *schedule);
esp_err_t load_step_calibration(void *steps);
esp_err_t store_step_calibration(void *steps);
esp_err_t load_hardware_state(void *hws);
esp_err_t store_hardware_state(void *hws);
esp_err_t load_device_name(void *name);
esp_err_t store_device_name(void *name);

#endif
