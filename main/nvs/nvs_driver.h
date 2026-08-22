#ifndef NVS_DRIVER_H
#define NVS_DRIVER_H

#include "prot.h"
#include <stdint.h>

esp_err_t load_schedule(doser_schedule *sched);
esp_err_t store_sched(doser_schedule *sched);
esp_err_t load_step_calibration(uint16_t *steps_per_ml);
esp_err_t store_step_calibration(uint16_t *steps_per_ml);
esp_err_t load_hardware_state(uint8_t *hardware_state);
esp_err_t store_hardware_state(uint8_t *hardware_state);

#endif
