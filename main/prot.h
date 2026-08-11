#ifndef PROT_H
#define PROT_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gptimer.h"

#define PIN_STEP 0
#define PIN_DIR 1
#define PIN_FAULTB 2
#define PIN_SLEEPB 3
#define PIN_ENABLE 4
#define PIN_LED1 5
#define PIN_LED2 6


typedef struct {
  uint16_t total_steps;
  uint16_t steps_achieved;
  uint8_t state;
  TaskHandle_t callback_task;
  gptimer_handle_t gptimer;
}step_struct;


#endif
