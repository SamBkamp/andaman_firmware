#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include <time.h>
#include <sys/time.h>

#include "esp_intr_alloc.h"
#include "esp_attr.h"
#include "driver/gptimer.h"
#include "stepper/step_util.h"
#include "wifi/wifi_driver.h"
#include "ble/ble_driver.h"
#include "prot.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "generic_util.h"

uint8_t wake_driver();
uint8_t sleep_driver();
void init_gpio_pins();

static const char *TAG = "ANDAMAN_DOSER";
static const char *error_activelow[] = {"ERROR", "OK"};

//https://github.com/espressif/esp-idf/blob/08e0d30a/components/esp_driver_gpio/include/driver/gpio.h
void app_main(void){
  doser_schedule sched = {
    .ml_per_dose = 0,
    .period_s = 60,
    .last_dose = 0
  };
  step_struct pump_step_data = {0};
  program_context ctx = {
    .hardware_states = 0,
    .schedule = &sched,
    .pump_step_data = &pump_step_data
  };

  init_gpio_pins();
  ble_init(&ctx);

  while(true){
    if((sched.last_dose + sched.period_s) < time(NULL) && sched.ml_per_dose > 0){
      gpio_set_level(PIN_LED2, 1);

      pump(sched.ml_per_dose, &pump_step_data);

      gpio_set_level(PIN_LED2, 0);

      sched.last_dose = time(NULL);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }


}
