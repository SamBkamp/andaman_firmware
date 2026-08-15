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

#define BLE_DEV_NAME "ADN-DOSER"
#define BLE_ADV_INTVL 0.625

int status_char_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args);
int dose_char_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args);

static const ble_uuid128_t doser_service_uuid = BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52,0xac,0x40,0xa9,0x2f,0xb1,0x68,0x76,0x8b);
static const ble_uuid128_t dosing_characteristic_uuid = BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52,0xD0,0x5E,0xa9,0x2f,0xb1,0x68,0x76,0x8b);
static const ble_uuid128_t status_characteristic_uuid = BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52,0x57,0x87,0xa9,0x2f,0xb1,0x68,0x76,0x8b);

static struct ble_gatt_chr_def characteristics[] = {
  {
    .uuid = &status_characteristic_uuid.u,
    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
    .access_cb = status_char_callback,
  },
  {
    .uuid = &dosing_characteristic_uuid.u,
    .flags = BLE_GATT_CHR_F_WRITE,
    .access_cb = dose_char_callback,
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


//only BLE entrypoint from the user, all other functions are called/registered here.
void ble_init(program_context *ctx){
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK( nvs_flash_erase() );
    ret = nvs_flash_init();
  }

  characteristics[0].arg = ctx->pump_step_data;
  characteristics[1].arg = ctx->pump_step_data;
  ESP_LOGI("BLE_INIT", "data: %d", ctx->pump_step_data->steps_achieved);

  ESP_ERROR_CHECK( ret );

  nimble_port_init();

  ESP_ERROR_CHECK(ble_gatts_count_cfg(gatt_service_definitions));
  ESP_ERROR_CHECK(ble_gatts_add_svcs(gatt_service_definitions));

  ble_hs_cfg.reset_cb = ble_on_reset;
  ble_hs_cfg.sync_cb = ble_on_sync;

  ble_svc_gap_init();
  ble_svc_gatt_init();

  nimble_port_freertos_init(nimble_host_run_task);

}


int status_char_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args){
  return 0;
}
int dose_char_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args){
  step_struct *pump_step_data = (step_struct *)args;
  uint8_t data[32];
  float mls;
  uint16_t len = OS_MBUF_PKTLEN(ctx->om);

  if(len > sizeof(data))
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

  int rc = ble_hs_mbuf_to_flat(ctx->om,
                               data,
                               sizeof(data),
                               NULL);
  data[len] = 0;
  mls = strtof((char *)data, NULL);
  ESP_LOGI(BLE_DEV_NAME, "GOT: %f", mls);
  pump(mls, pump_step_data);
  ESP_LOGI(BLE_DEV_NAME, "DONE");
  return 0;
}
