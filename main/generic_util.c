#include "generic_util.h"
#include <time.h>
#include "prot.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"
#include "esp_attr.h"
#include "driver/gpio.h"
#include <stdint.h>
#include "esp_netif_sntp.h"

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
