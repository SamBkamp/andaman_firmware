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

typedef struct{
  uint8_t v[3];
}version;

static const version SOFTWARE_VERSION = {.v = {1,0,1}};
static const version BOARD_VERSION = {.v = {1,0,1}};

int schedule_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args);
int manual_dose(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args);
int device_information(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args);
int step_direction_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args);
int calibration_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void *args);

static const ble_uuid128_t doser_service_uuid = \
  BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52,0xac,0x40,0xa9,0x2f,0xb1,0x68,0x76,0x8b);

//                                                          VVVVVVVVV  characteristic identifier
static const ble_uuid128_t dosing_characteristic_uuid = \
  BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52, 0xD0,0x5E, 0xa9,0x2f,0xb1,0x68,0x76,0x8b);

static const ble_uuid128_t schedule_characteristic_uuid = \
  BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52, 0x5C,0xED, 0xa9,0x2f,0xb1,0x68,0x76,0x8b);

static const ble_uuid128_t device_info_uuid = \
  BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52, 0x13,0xF0, 0xa9,0x2f,0xb1,0x68,0x76,0x8b);

static const ble_uuid128_t calibration_const_uuid = \
  BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52, 0xCA,0x1B, 0xa9,0x2f,0xb1,0x68,0x76,0x8b);

static const ble_uuid128_t write_direction_uuid = \
  BLE_UUID128_INIT(0x96,0xe8,0x1e,0x1d,0xA5,0x1A,0x08,0x52, 0xD1,0x4E, 0xa9,0x2f,0xb1,0x68,0x76,0x8b);

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



//only BLE entrypoint from the user, all other functions are called/registered here.
void ble_init(program_context *ctx){
  //this needs to be here because its evaluated at runtime
  //make characteristics[1] take ctx and then loopify this
  characteristics[0].arg = ctx;
  characteristics[1].arg = ctx;
  characteristics[2].arg = ctx;
  characteristics[3].arg = ctx;
  characteristics[4].arg = ctx;

  nimble_port_init();

  ESP_ERROR_CHECK(ble_gatts_count_cfg(gatt_service_definitions));
  ESP_ERROR_CHECK(ble_gatts_add_svcs(gatt_service_definitions));

  ble_hs_cfg.reset_cb = ble_on_reset;
  ble_hs_cfg.sync_cb = ble_on_sync;

  ble_svc_gap_init();
  ble_svc_gatt_init();

  nimble_port_freertos_init(nimble_host_run_task);

}


int set_schedule(struct ble_gatt_access_ctxt *ctx, void* args){
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

  //BEWARE OF TRUNCATION: ULONG >= 32bits, period_s is 16 bits
  uint16_t new_period = (uint16_t)strtol(post_ptr, NULL, 10);
  if(new_period < 1)
    return BLE_ATT_ERR_VALUE_NOT_ALLOWED;

  p_ctx->schedule->ml_per_dose = strtof(data, NULL);
  p_ctx->schedule->period_s = new_period;

  //commit new schedule to NVS
  ESP_ERROR_CHECK(store_sched(p_ctx->schedule));

  return 0;
}

int read_schedule(struct ble_gatt_access_ctxt *ctx, void* args){
  program_context *p_ctx = (program_context *)args;
  char data[32];
  int len = snprintf(data, 32, "%.3f,%d", p_ctx->schedule->ml_per_dose, p_ctx->schedule->period_s);

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

int manual_dose(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args){
  program_context *p_ctx = (program_context *)args;
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

  if(mls == 0 || mls == ERANGE)
    return BLE_ATT_ERR_VALUE_NOT_ALLOWED;

  pump(mls, p_ctx->pump_step_data);
  return 0;
}

int write_calibration_data(struct ble_gatt_access_ctxt *ctx, void *args){
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

int read_calibration_data(struct ble_gatt_access_ctxt *ctx, void* args){
  program_context *p_ctx = (program_context *)args;

  return os_mbuf_append(ctx->om,
                        &p_ctx->pump_step_data->steps_per_ml,
                        sizeof(p_ctx->pump_step_data->steps_per_ml))
    == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  //sorry for this atrocious formatting
}


int calibration_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void *args){
  switch (ctx->op) {

  case BLE_GATT_ACCESS_OP_READ_CHR:
    return read_calibration_data(ctx, args);
    break;

  case BLE_GATT_ACCESS_OP_WRITE_CHR:
    return write_calibration_data(ctx, args);
    break;

  default:
    return BLE_ATT_ERR_UNLIKELY;
  }

  return 0;
}



int schedule_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void *args){

  switch (ctx->op) {

  case BLE_GATT_ACCESS_OP_READ_CHR:
    return read_schedule(ctx, args);
    break;

  case BLE_GATT_ACCESS_OP_WRITE_CHR:
    return set_schedule(ctx, args);
    break;

  default:
    return BLE_ATT_ERR_UNLIKELY;
  }

  return 0;
}


int write_step_direction(struct ble_gatt_access_ctxt *ctx, void* args){
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
  data = data << PC_STEP_DIRECTION_PIN;
  p_ctx->hardware_states &= ~(PC_STEP_DIRECTION); //clear dir bit
  p_ctx->hardware_states |= data;
  ESP_LOGI("BLE", "hw states: %d", p_ctx->hardware_states);
  ESP_ERROR_CHECK(store_hardware_state(&(p_ctx->hardware_states)));

  return 0;

}


int read_step_direction(struct ble_gatt_access_ctxt *ctx, void *args){

  program_context *p_ctx = (program_context *)args;
  char data[32];
  int len;

  switch(p_ctx->hardware_states & PC_STEP_DIRECTION){
  case PC_STEP_DIRECTION:
    len = snprintf(data, 32, "CCW");
    break;
  default:
    len = snprintf(data, 32, "CW");
    break;
  }

  return os_mbuf_append(ctx->om, data, len) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;


}

int step_direction_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args){

  switch (ctx->op) {

  case BLE_GATT_ACCESS_OP_READ_CHR:
    return read_step_direction(ctx, args);
    break;

  case BLE_GATT_ACCESS_OP_WRITE_CHR:
    return write_step_direction(ctx, args);
    break;

  default:
    return BLE_ATT_ERR_UNLIKELY;
  }

  return 0;
}
