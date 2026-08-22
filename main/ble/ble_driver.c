#include <string.h>
#include <stdio.h>
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

typedef struct{
  uint8_t v[3];
}version;

static const version SOFTWARE_VERSION = {.v = {0,0,1}};
static const version BOARD_VERSION = {.v = {0,0,1}};

int status_char_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args);
int dose_char_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args);
int set_new_sched_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args);
int device_information(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args);
int read_calibration_data(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args);
int write_calibration_data(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args);
int write_step_direction(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args);

static const ble_uuid128_t doser_service_uuid = \
  BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52,0xac,0x40,0xa9,0x2f,0xb1,0x68,0x76,0x8b);

//                                                          VVVVVVVVV  characteristic identifier
static const ble_uuid128_t dosing_characteristic_uuid = \
  BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52, 0xD0,0x5E, 0xa9,0x2f,0xb1,0x68,0x76,0x8b);

static const ble_uuid128_t status_characteristic_uuid =\
  BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52, 0x57,0x87, 0xa9,0x2f,0xb1,0x68,0x76,0x8b);

static const ble_uuid128_t sched_characteristic_uuid = \
  BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52, 0x5C,0xED, 0xa9,0x2f,0xb1,0x68,0x76,0x8b);

static const ble_uuid128_t device_info_uuid = \
  BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52, 0x13,0xF0, 0xa9,0x2f,0xb1,0x68,0x76,0x8b);

static const ble_uuid128_t calibration_info_uuid = \
  BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52, 0xCA,0x1B, 0xa9,0x2f,0xb1,0x68,0x76,0x8b);

static const ble_uuid128_t write_calibration_uuid = \
  BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52, 0xCA,0x00, 0xa9,0x2f,0xb1,0x68,0x76,0x8b);

static const ble_uuid128_t write_direction_uuid = \
  BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52, 0xD1,0x4E, 0xa9,0x2f,0xb1,0x68,0x76,0x8b);

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
  {
    .uuid = &sched_characteristic_uuid.u,
    .flags = BLE_GATT_CHR_F_WRITE,
    .access_cb = set_new_sched_callback,
  },
  {
    .uuid = &device_info_uuid.u,
    .flags = BLE_GATT_CHR_F_READ,
    .access_cb = device_information,
  },
  {
    .uuid = &calibration_info_uuid.u,
    .flags = BLE_GATT_CHR_F_READ,
    .access_cb = read_calibration_data,
  },
  {
    .uuid = &write_calibration_uuid.u,
    .flags = BLE_GATT_CHR_F_WRITE,
    .access_cb = write_calibration_data,
  },
  {
    .uuid = &write_direction_uuid.u,
    .flags = BLE_GATT_CHR_F_WRITE,
    .access_cb = write_step_direction,
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
  //this needs to be here because its evaluated at runtime
  //make characteristics[1] take ctx and then loopify this
  characteristics[0].arg = ctx;
  characteristics[1].arg = ctx->pump_step_data;
  characteristics[2].arg = ctx;
  characteristics[3].arg = ctx;
  characteristics[4].arg = ctx;
  characteristics[5].arg = ctx;
  characteristics[6].arg = ctx;

  nimble_port_init();

  ESP_ERROR_CHECK(ble_gatts_count_cfg(gatt_service_definitions));
  ESP_ERROR_CHECK(ble_gatts_add_svcs(gatt_service_definitions));

  ble_hs_cfg.reset_cb = ble_on_reset;
  ble_hs_cfg.sync_cb = ble_on_sync;

  ble_svc_gap_init();
  ble_svc_gatt_init();

  nimble_port_freertos_init(nimble_host_run_task);

}


int set_new_sched_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args){
  program_context *p_ctx = (program_context *)args;
  char data[32];
  float mls_per_dose = 0;
  uint16_t period = 0;
  uint16_t len = OS_MBUF_PKTLEN(ctx->om);

  if(len > sizeof(data))
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

  ble_hs_mbuf_to_flat(ctx->om, data, sizeof(data), NULL);
  data[len] = 0;

  char *post_ptr = data;
  uint8_t i = 0;
  for(; data[i] != 0 && data[i] != ','; i++){}

  data[i++] = 0; //set the comma to a 0 and increment postfix
  post_ptr = &data[i]; //ptr now points to first char in substr after comma

  p_ctx->schedule->ml_per_dose = strtof(data, NULL);
  p_ctx->schedule->period_s = (uint16_t)strtol(post_ptr, NULL, 10);
  //BEWARE OF TRUNCATION: ULONG >= 32bits, period_s is 16 bits

  //commit new schedule to NVS
  ESP_ERROR_CHECK(store_sched(p_ctx->schedule));

  return 0;
}

int status_char_callback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args){
  program_context *p_ctx = (program_context *)args;
  char data[32];
  int len = snprintf(data, 32, "dosing %.3f every %d seconds", p_ctx->schedule->ml_per_dose, p_ctx->schedule->period_s);

  return os_mbuf_append(ctx->om, data, len) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

int device_information(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args){
  char data[32];
  data[0] = SOFTWARE_VERSION.v[0];
  data[1] = SOFTWARE_VERSION.v[1];
  data[2] = SOFTWARE_VERSION.v[2];
  data[3] = BOARD_VERSION.v[0];
  data[4] = BOARD_VERSION.v[1];
  data[5] = BOARD_VERSION.v[2];

  return os_mbuf_append(ctx->om, data, sizeof(version)*2) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
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
  pump(mls, pump_step_data);
  return 0;
}

int write_calibration_data(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args){
  program_context *p_ctx = (program_context *)args;
  uint16_t len = OS_MBUF_PKTLEN(ctx->om);
  char data[32];

  if(len > sizeof(data)){
    ESP_LOGE("BLE", "PACKET_SIZE_WRONG");
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  }

  ble_hs_mbuf_to_flat(ctx->om, data, sizeof(data), NULL);
  data[len] = 0;

  p_ctx->pump_step_data->steps_per_ml = (uint16_t)strtol(data, NULL, 10);

  ESP_LOGI("BLE", "got %d", p_ctx->pump_step_data->steps_per_ml);

  ESP_ERROR_CHECK(store_step_calibration(&p_ctx->pump_step_data->steps_per_ml));
  return 0;
}

int read_calibration_data(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args){
  program_context *p_ctx = (program_context *)args;

  return os_mbuf_append(ctx->om,
                        &p_ctx->pump_step_data->steps_per_ml,
                        sizeof(p_ctx->pump_step_data->steps_per_ml))
    == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  //sorry for this atrocious formatting
}

int write_step_direction(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args){
  program_context *p_ctx = (program_context *)args;
  uint16_t len = OS_MBUF_PKTLEN(ctx->om);
  char data;

  if(len > sizeof(data)){
    ESP_LOGE("BLE", "PACKET_SIZE_WRONG");
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  }

  ble_hs_mbuf_to_flat(ctx->om, &data, sizeof(data), NULL);

  //we just want the LSB
  data &= 1;
  gpio_set_level(PIN_DIR, data);
  data = data << PC_STEP_DIRECTION;
  p_ctx->hardware_states = data;
  ESP_LOGI("BLE", "hw states: %d", p_ctx->hardware_states);

  return 0;

}
