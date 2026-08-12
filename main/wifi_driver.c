#include <stdint.h>
#include "esp_wifi.h"
#include "prot.h"
#include "esp_err.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "nvs_flash.h"

#define WIFI_SSID
#define WIFI_PW 

//https://github.com/espressif/esp-idf/blob/v6.0.2/examples/wifi/getting_started/station/main/station_example_main.c

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data){

  if(event_base == WIFI_EVENT){
    switch(event_id){
    case WIFI_EVENT_STA_START:
      ESP_LOGI("ANDAMAN_WIFI", "Wifi started");
      ESP_ERROR_CHECK(esp_wifi_connect());
      break;
    case WIFI_EVENT_STA_CONNECTED:
      ESP_LOGI("ANDAMAN_WIFI", "connected to AP");
      break;
    case WIFI_EVENT_STA_DISCONNECTED:
      ESP_LOGI("ANDAMAN_WIFI", "Disconnected");
      break;
    }
  }


  if(event_base == IP_EVENT){
    switch(event_id){
    case IP_EVENT_STA_GOT_IP:
      ESP_LOGI("ANDAMAN_WIFI", "GOT IP");
      break;
    }
  }



}


int wifi_start(void){
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK( nvs_flash_erase() );
    ret = nvs_flash_init();
  }
    ESP_ERROR_CHECK( ret );
  wifi_config_t wcfg = {
    .sta = {
      .ssid = WIFI_SSID,
      .password = WIFI_PW,
      .failure_retry_cnt = 10
    }
  };
  wifi_init_config_t icfg = WIFI_INIT_CONFIG_DEFAULT();

  //ESP_ERROR_CHECK(esp_netif_init());
  //esp_netif_create_default_wifi_sta();

  ESP_ERROR_CHECK(esp_wifi_init(&icfg));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));

  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI("ANDAMAN_DOSER", "WIFI INITIALISED");


  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                             &wifi_event_handler, NULL);

  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                             &wifi_event_handler, NULL);

  //ESP_ERROR_CHECK(esp_wifi_connect());
  return 0;
}
