]#include <stdio.h>
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

#include "esp_sntp.h"
#include "esp_intr_alloc.h"
#include "esp_attr.h"
#include "driver/gptimer.h"
#include "step_util.h"
#include "wifi_driver.h"
#include "prot.h"


#define GPIO_OUTPUT_PIN_REG (1ULL<<PIN_STEP) | (1ULL<<PIN_DIR)          \
  |  (1ULL<<PIN_SLEEPB) | (1ULL<<PIN_ENABLE)                            \
  | (1ULL<<PIN_LED1) | (1ULL<<PIN_LED2)

#define GPIO_INPUT_PIN_MASK (1ULL<<PIN_FAULTB)
#define IOBUF_SIZE 512


uint8_t wake_driver();
uint8_t sleep_driver();

static const char *TAG = "ANDAMAN_DOSER";
static const char *error_activelow[] = {"ERROR", "OK"};


void set_sys_time(void){
  struct tm tm;
  tm.tm_year = 2026 - 1900;
  tm.tm_mon = 7;
  tm.tm_mday = 11;
  tm.tm_hour = 9;
  tm.tm_min = 33;
  tm.tm_sec = 10;
  //time_t t = mktime(&tm);
  time_t t = 1786469970L;
  //ESP_LOGI(TAG, "Setting time: %s", asctime(&tm));
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);
}

//https://github.com/espressif/esp-idf/blob/08e0d30a/components/esp_driver_gpio/include/driver/gpio.h
void app_main(void){
  time_t now;
  char strftime_buf[64];
  struct tm timeinfo;
  doser_schedule sched = {
    .ml_per_dose = 2,
    .period_s = 60,
    .last_dose = 0
  };  
  
  step_struct pump_step_data = {0};
  char iobuf[IOBUF_SIZE];
  gpio_config_t output_io_conf = {
    .intr_type = GPIO_INTR_DISABLE,
    .mode = GPIO_MODE_OUTPUT,
    .pin_bit_mask = GPIO_OUTPUT_PIN_REG,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .pull_up_en = GPIO_PULLUP_DISABLE
  };
  gpio_config(&output_io_conf);

  gpio_config_t input_io_conf = {
    .intr_type = GPIO_INTR_DISABLE,
    .mode = GPIO_MODE_INPUT,
    .pin_bit_mask = GPIO_INPUT_PIN_MASK,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .pull_up_en = GPIO_PULLUP_ENABLE
  };
  gpio_config(&input_io_conf);

  ESP_LOGI(TAG, "HAIII o/");

  set_sys_time();
  time(&now);
  setenv("TZ", "UTC+8", 1);
  tzset();

  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);


  wifi_start();


//  while(true){
//    if((sched.last_dose + sched.period_s) < time(&now)){
//      //wakeup driver
//      ESP_LOGI(TAG, "driver started");       
//      ESP_LOGI(TAG, "driver state: %s", error_activelow[wake_driver()]);
//      gpio_set_level(PIN_LED2, 1);
//      pump(sched.ml_per_dose, &pump_step_data);
//      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);//wait for timer isr to finish
//      sleep_driver();
//      gpio_set_level(PIN_LED2, 0);
//      sched.last_dose = time(NULL);
//    }
//    vTaskDelay(pdMS_TO_TICKS(2000));
//  }

  
}


uint8_t wake_driver(){
  gpio_set_level(PIN_SLEEPB, 1);
  gpio_set_level(PIN_ENABLE, 1);
  vTaskDelay(pdMS_TO_TICKS(50)); //driver needs a little bit of time to startup
  return gpio_get_level(PIN_FAULTB);
}

uint8_t sleep_driver(){
  //driver sleep
  gpio_set_level(PIN_SLEEPB, 0);
  gpio_set_level(PIN_ENABLE, 0);
  vTaskDelay(pdMS_TO_TICKS(50)); //driver needs a little bit of time to startup
  return gpio_get_level(PIN_FAULTB);
}
