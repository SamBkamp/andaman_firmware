#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "ble_cb.h"
#include "esp_nimble_hci.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"


#define BLE_DEV_NAME "ADN-DOSER"
#define BLE_ADV_INTVL 0.625

static void advertising(void){
  struct ble_gap_adv_params adv_params = {0};
  struct ble_hs_adv_fields adv_fields = {0};

  adv_fields.name = (uint8_t *)BLE_DEV_NAME;
  adv_fields.name_len = sizeof(BLE_DEV_NAME);
  adv_fields.name_is_complete = 1;

  ble_gap_adv_set_fields(&adv_fields);


  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  adv_params.itvl_min = (unsigned int)(500/BLE_ADV_INTVL); //converts and rounds down to closest published adv interval
  adv_params.itvl_max = (unsigned int)(500/BLE_ADV_INTVL); //converts and rounds down to closest published adv interval

  ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, gap_event, NULL);

}


int gap_event(struct ble_gap_event *event, void *arg){
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

void ble_on_sync(void){
  ESP_LOGI("AD_BLE", "BLE host sync'd");
  advertising();
}
void ble_on_reset(int reason){
  ESP_LOGI("AD_BLE", "BLE host reset (%d)", reason);
}

void nimble_host_run_task(void *params){
  nimble_port_run();
  nimble_port_freertos_deinit();
  vTaskDelete(NULL);
}
