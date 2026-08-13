#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "esp_nimble_hci.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"

#define BLE_DEV_NAME "ADN-DOSER"

static void advertising(void);

static int gap_event(struct ble_gap_event *event, void *arg){
  switch(event->type){
  case BLE_GAP_EVENT_CONNECT:
    if (event->connect.status == 0) ESP_LOGI("AD_BLE", "Client connected");
    else{
      ESP_LOGI("AD_BLE", "Connection failed: %d", event->connect.status);
      // Start advertising again
      advertising();
    }
    break;
  case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGI("AD_BLE", "Client disconnected");

    // Start advertising again so another client can connect
    advertising();
    break;
  case BLE_GAP_EVENT_ADV_COMPLETE:
    ESP_LOGI("AD_BLE", "Advertising complete");

    // Usually restart advertising if you want to remain discoverable
    advertising();
  default:
    break;
  }
  return 0;
}


static void advertising(void){
  struct ble_gap_adv_params adv_params = {0};
  struct ble_hs_adv_fields adv_fields = {0};

  adv_fields.name = (uint8_t *)BLE_DEV_NAME;
  adv_fields.name_len = sizeof(BLE_DEV_NAME);
  adv_fields.name_is_complete = 1;

  ble_gap_adv_set_fields(&adv_fields);


  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, gap_event, NULL);

}


static void ble_on_sync(void){
  ESP_LOGI("AD_BLE", "BLE host sync'd");
  advertising();
}



static void ble_on_reset(int reason){
  ESP_LOGI("AD_BLE", "BLE host reset (%d)", reason);
}


static void nimble_host_run_task(void *params){

  nimble_port_run();
  nimble_port_freertos_deinit();
  vTaskDelete(NULL);
}

void ble_init(void){
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK( nvs_flash_erase() );
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK( ret );

  nimble_port_init();

  ble_hs_cfg.reset_cb = ble_on_reset;
  ble_hs_cfg.sync_cb = ble_on_sync;

  ble_svc_gap_init();
  ble_svc_gatt_init();

  nimble_port_freertos_init(nimble_host_run_task);

}
