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

#define GPIO_OUTPUT_PIN_REG (1ULL<<PIN_STEP) | (1ULL<<PIN_DIR)          \
  |  (1ULL<<PIN_SLEEPB) | (1ULL<<PIN_ENABLE)                            \
  | (1ULL<<PIN_LED1) | (1ULL<<PIN_LED2)

#define GPIO_INPUT_PIN_MASK (1ULL<<PIN_FAULTB)
#define IOBUF_SIZE 512


#define SEC_PER_MIN 60
#define SEC_PER_HR 3600
#define AD_HOUR_TO_SECS(secs) secs * SEC_PER_HR

typedef struct {
  uint16_t total_steps;
  uint16_t steps_achieved;
  uint8_t state;
  TaskHandle_t callback_task;
  gptimer_handle_t gptimer;
  uint16_t steps_per_ml;
}step_struct;


typedef struct {
  float ml_per_dose;
  uint16_t period_s;
  time_t last_dose;
}doser_schedule;


//hardware state masks
#define PC_WIFI_ACTIVE 1 << 0
#define PC_BLE_ACTIVCE 1 << 1
#define PC_TIMER_INIT 1 << 2
#define PC_NVS_INIT 1 << 3
#define PC_STEP_DIRECTION 1 << 4

typedef struct{
  uint8_t hardware_states;
  doser_schedule *schedule;
  step_struct *pump_step_data;  
}program_context;

uint8_t wake_driver();
uint8_t sleep_driver();

#endif
