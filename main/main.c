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

#include "esp_intr_alloc.h"
#include "esp_attr.h"
#include "driver/gptimer.h"
#include "step_util.h"
#include "prot.h"



#define GPIO_OUTPUT_PIN_REG (1ULL<<PIN_STEP) | (1ULL<<PIN_DIR)          \
  |  (1ULL<<PIN_SLEEPB) | (1ULL<<PIN_ENABLE)                            \
  | (1ULL<<PIN_LED1) | (1ULL<<PIN_LED2)

#define GPIO_INPUT_PIN_MASK (1ULL<<PIN_FAULTB)
#define IOBUF_SIZE 512


static const char *TAG = "ANDAMAN_DOSER";
static const char *error_activelow[] = {"ERROR", "OK"};


//https://github.com/espressif/esp-idf/blob/08e0d30a/components/esp_driver_gpio/include/driver/gpio.h
void app_main(void){
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



  //wakeup driver
  gpio_set_level(PIN_SLEEPB, 1);
  gpio_set_level(PIN_ENABLE, 1);

  ESP_LOGI(TAG, "driver started");
  ESP_LOGI(TAG, "driver state: %s", error_activelow[gpio_get_level(PIN_FAULTB)]);


  pump(1, &pump_step_data);


  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);//wait for timer isr to finish

  gpio_set_level(PIN_SLEEPB, 0);
  gpio_set_level(PIN_ENABLE, 0);

}
