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
#include "step_util.h"
#include "wifi_driver.h"
#include "ble_driver.h"
#include "prot.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"


#define GPIO_OUTPUT_PIN_REG (1ULL<<PIN_STEP) | (1ULL<<PIN_DIR)          \
  |  (1ULL<<PIN_SLEEPB) | (1ULL<<PIN_ENABLE)                            \
  | (1ULL<<PIN_LED1) | (1ULL<<PIN_LED2)

#define GPIO_INPUT_PIN_MASK (1ULL<<PIN_FAULTB)
#define IOBUF_SIZE 512


uint8_t wake_driver();
uint8_t sleep_driver();
void init_gpio_pins();

static const char *TAG = "ANDAMAN_DOSER";
static const char *error_activelow[] = {"ERROR", "OK"};


void update_sys_time(void){
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  esp_netif_sntp_init(&config);
  ESP_ERROR_CHECK(esp_netif_sntp_sync_wait(pdMS_TO_TICKS(3000)));
}

void print_time(void){
  time_t now;
  char strftime_buf[64];
  struct tm timeinfo;

  time(&now);
  setenv("TZ", "UTC+8", 1);
  tzset();
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(TAG, "The current date/time in Hong Kong is: %s", strftime_buf);
  
}

//https://github.com/espressif/esp-idf/blob/08e0d30a/components/esp_driver_gpio/include/driver/gpio.h
void app_main(void){
  doser_schedule sched = {
    .ml_per_dose = 2,
    .period_s = 60,
    .last_dose = 0
  };
  step_struct pump_step_data = {0};


  init_gpio_pins();
  //wifi_start();
  //update_sys_time();
  //esp_wifi_disconnect();
  //esp_wifi_stop();
  ble_init();
  


  while(true){
//    if((sched.last_dose + sched.period_s) < time(NULL)){
//      wake_driver();
//      gpio_set_level(PIN_LED2, 1);
//
//      pump(sched.ml_per_dose, &pump_step_data);
//      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);//wait for timer isr to finish
//
//      sleep_driver();
//      gpio_set_level(PIN_LED2, 0);
//
//      sched.last_dose = time(NULL);
//    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }


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


void init_gpio_pins(){
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
}
