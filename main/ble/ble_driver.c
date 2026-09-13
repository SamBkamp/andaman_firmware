#include <string.h>
#include <stdio.h>
#include <errno.h>
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
#include "ble_driver.h"
#include "stepper/step_util.h"
#include "ble_cb.h"
#include "prot.h"
#include "driver/gpio.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "nvs/nvs_driver.h"

#define BLE_DEV_NAME "ADN-DOSER"
#define BLE_ADV_INTVL 0.625

static char *BLE_device_name; // <- stupid fucking bodge because I can't pass data to the on_sync function


static struct ble_gatt_chr_def characteristics[] = {
  {
    .uuid = &dosing_characteristic_uuid.u,
    .flags = BLE_GATT_CHR_F_WRITE,
    .access_cb = manual_dose,
  },
  {
    .uuid = &schedule_characteristic_uuid.u,
    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
    .access_cb = schedule_handler,
  },
  {
    .uuid = &device_info_uuid.u,
    .flags = BLE_GATT_CHR_F_READ,
    .access_cb = device_information,
  },
  {
    .uuid = &calibration_const_uuid.u,
    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
    .access_cb = calibration_handler,
  },
  {
    .uuid = &write_direction_uuid.u,
    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
    .access_cb = step_direction_handler,
  },
  {0}
};


static struct ble_gatt_svc_def gatt_service_definitions[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &doser_service_uuid.u,
    .characteristics = characteristics
  },
  {0}
};



void ble_on_sync(void){
  ESP_LOGI("AD_BLE", "BLE host sync'd");
  advertising(BLE_device_name);
}
void ble_on_reset(int reason){
  ESP_LOGI("AD_BLE", "BLE host reset (%d)", reason);
}

void nimble_host_run_task(void *params){
  nimble_port_run();
  nimble_port_freertos_deinit();
  vTaskDelete(NULL);
}



//only BLE entrypoint from the user, all other functions are called/registered here.
void ble_init(program_context *ctx){
  //this needs to be here because its evaluated at runtime
  //make characteristics[1] take ctx and then loopify this
  characteristics[0].arg = ctx;
  characteristics[1].arg = ctx;
  characteristics[2].arg = ctx;
  characteristics[3].arg = ctx;
  characteristics[4].arg = ctx;

  BLE_device_name = ctx->BLE_device_name;

  nimble_port_init();

  ESP_ERROR_CHECK(ble_gatts_count_cfg(gatt_service_definitions));
  ESP_ERROR_CHECK(ble_gatts_add_svcs(gatt_service_definitions));

  ble_hs_cfg.reset_cb = ble_on_reset;
  ble_hs_cfg.sync_cb = ble_on_sync;

  ble_svc_gap_init();
  ble_svc_gatt_init();

  nimble_port_freertos_init(nimble_host_run_task);

}

void advertising(char *device_name){
  struct ble_gap_adv_params adv_params = {0};
  struct ble_hs_adv_fields adv_fields = {0};

  adv_fields.name = (uint8_t *)device_name;
  adv_fields.name_len = strlen(device_name);
  adv_fields.name_is_complete = 1;

  ble_gap_adv_set_fields(&adv_fields);


  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  adv_params.itvl_min = (unsigned int)(500/BLE_ADV_INTVL); //converts and rounds down to closest published adv interval
  adv_params.itvl_max = (unsigned int)(500/BLE_ADV_INTVL); //converts and rounds down to closest published adv interval

  ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, gap_event, device_name);

}


int gap_event(struct ble_gap_event *event, void *arg){
  switch(event->type){
  case BLE_GAP_EVENT_CONNECT:
    if (event->connect.status == 0) ESP_LOGI("AD_BLE", "Client connected");
    else{
      ESP_LOGI("AD_BLE", "Connection failed: %d", event->connect.status);
      // Start advertising again
      advertising((char *)arg);
    }
    break;
  case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGI("AD_BLE", "Client disconnected");

    // Start advertising again so another client can connect
    advertising((char *)arg);
    break;
  case BLE_GAP_EVENT_ADV_COMPLETE:
    ESP_LOGI("AD_BLE", "Advertising complete");

    // Usually restart advertising if you want to remain discoverable
    advertising((char *)arg);
  default:
    break;
  }
  return 0;
}
