#ifndef PROT_H
#define PROT_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gptimer.h"
#include <time.h>

#define PIN_STEP 0
#define PIN_DIR 1
#define PIN_FAULTB 2
#define PIN_SLEEPB 3
#define PIN_ENABLE 4
#define PIN_LED1 5
#define PIN_LED2 6


#define SEC_PER_MIN 60
#define SEC_PER_HR 3600
#define AD_HOUR_TO_SECS(secs) secs * SEC_PER_HR

typedef struct {
  uint16_t total_steps;
  uint16_t steps_achieved;
  uint8_t state;
  TaskHandle_t callback_task;
  gptimer_handle_t gptimer;
}step_struct;


typedef struct {
  float ml_per_dose;
  uint16_t period_s;
  time_t last_dose;
}doser_schedule;

#endif
