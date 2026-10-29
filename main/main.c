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
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs/nvs_driver.h"

#include "driver/temperature_sensor.h"

uint8_t wake_driver();
uint8_t sleep_driver();
void init_gpio_pins();

static const char *TAG = "ANDAMAN_DOSER";
static const char *error_activelow[] = {"ERROR", "OK"};

//https://github.com/espressif/esp-idf/blob/08e0d30a/components/esp_driver_gpio/include/driver/gpio.h
void app_main(void){
  temperature_sensor_handle_t temp_handle = NULL;
  float temp;
  doser_schedule sched = {
    .ml_per_dose = 0,
    .period_s = 60,
    .last_dose = 0,
    .mode = INTERVAL,
    .timed = {
      .count = 0,
      .flag_bitmask = 0
    }
  };
  step_struct pump_step_data = {
    .steps_per_ml = DEFAULT_STEP_CALIBRATION
  };
  program_context ctx = {
    .hardware_states = 0,
    .schedule = &sched,
    .pump_step_data = &pump_step_data,
    .BLE_device_name = "ADN-Doser",
    .total_amount_dosed = 0
  };
  temperature_sensor_config_t temp_sensor_config = {
    .range_min = 20,
    .range_max = 100,
  };

  dose_time_data_t d_time = {
    .hour = 00,
    .minute = 47,
    .ml = 2.74
  };


  gpio_set_level(PIN_LED_GEN, 1);
  gpio_set_level(PIN_LED_ERROR, 1);
  ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_handle));
  ESP_ERROR_CHECK(temperature_sensor_enable(temp_handle));

  esp_err_t nvs_ret = nvs_flash_init();

  /* if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) */
  /*   ESP_ERROR_CHECK(nvs_flash_erase()); */

  //load initialisation data from NVS
  load_or_default(load_tz, store_tz, &ctx.tz, "UTC-8");
  load_or_default(load_schedule, store_sched, &sched, &sched);
  load_or_default(load_step_calibration, store_step_calibration, &pump_step_data.steps_per_ml, &pump_step_data.steps_per_ml);
  load_or_default(load_hardware_state, store_hardware_state, &ctx.hardware_states, &ctx.hardware_states);
  load_or_default(load_device_name, store_device_name, &ctx.BLE_device_name, &ctx.BLE_device_name);
  load_or_default(load_systime, store_systime, NULL, NULL);

  print_time(&ctx);
  ESP_LOGI(TAG, "Device name: %s", ctx.BLE_device_name);

  ctx.hardware_states &= ~(PC_PUMP_ACTIVE); //turn off active on restart
  //the pump can't be active but it may have been stored that way if pump lost power while pumping
  sched.last_dose = 0; //so the schedule starts executing from now. Time independant as we might not have a a good time source on each boot

  init_gpio_pins();
  ble_init(&ctx);
  //set default pin outputs
  gpio_set_level(PIN_DIR, (ctx.hardware_states & PC_STEP_DIRECTION)>>PC_STEP_DIRECTION_PIN);
  gpio_set_level(PIN_LED_GEN, 0);
  gpio_set_level(PIN_LED_ERROR, gpio_get_level(PIN_FAULTB) ^ 1);
  gpio_set_level(PIN_LOWI_MODE, 0);



  //TEMPORARILY DISABLED THANKS TO THERMAL ISSUES LOL
  //on startup, check if the saved schedule is a continious one
//  if(sched.mode == CONTINUOUS){
//    pump_continuous(sched.ml_per_dose, &ctx);
//  }

  while(true){
    if (temperature_sensor_get_celsius(temp_handle, &temp) == ESP_OK) {
      if(temp > 55) gpio_set_level(PIN_LOWI_MODE, 1);
      //ESP_LOGI("TEMP", "ESP32 temperature: %.2f C", temp);
      //printf("%.2f C\n", temp);
      //fflush(stdout);
    }

    switch(sched.mode){
    case INTERVAL:
      if((sched.last_dose + sched.period_s) < time(NULL)
         && sched.ml_per_dose > 0){
        gpio_set_level(PIN_LED2, 1);
        sched.last_dose = time(NULL);
        pump(sched.ml_per_dose, &ctx);
      }
      break;
    case TIME:
      time_t timestamp = time(NULL);
      struct tm timeinfo;
      localtime_r(&timestamp, &timeinfo);

      int hours = timeinfo.tm_hour;
      int minutes = timeinfo.tm_min;

      for(uint8_t i = 0; i < sched.timed.count; i++){
        uint16_t mask = 1 << i;
        if(sched.timed.times[i].hour == hours
           && sched.timed.times[i].minute == minutes
           && (sched.timed.flag_bitmask & mask) == 0){
          pump(sched.timed.times[i].ml, &ctx);
          sched.timed.flag_bitmask |= mask;
        }
      }

      break;

    default:
      break;
    }


    vTaskDelay(pdMS_TO_TICKS(2000));
  }

}
