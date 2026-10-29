#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "prot.h"
#include "ble_cb.h"
#include "esp_nimble_hci.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "nvs/nvs_driver.h"
#include "stepper/step_util.h"
#include "OTA/ota_driver.h"
#include "generic_util.h"

#include "driver/gpio.h"

#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>


#define BLE_DEV_NAME "ADN-DOSER"
#define BLE_ADV_INTVL 0.625

static const version SOFTWARE_VERSION = {.v = {1,3,3}};
static const version BOARD_VERSION = {.v = {0,1,2}};
static const version BATCH_NUMBER = {.v = {0,0,1}};


int continuous_pumping_handler(program_context *p_ctx, char *data){
  return 0; //EARLY RETURN, THIS FUNCTIONALITY IS TEMPORARILY REMOVED

  errno = 0;
  float ml_per_min = strtof(&data[1], NULL);
  ESP_LOGI("ADN_BLE", "cont. dosing %f ml/min", ml_per_min);
  if(ml_per_min == EINVAL || ml_per_min < 1)
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

  p_ctx->schedule->ml_per_dose = ml_per_min;
  p_ctx->schedule->mode = CONTINUOUS;
  pump_continuous(ml_per_min, p_ctx);

  ESP_ERROR_CHECK(store_sched(p_ctx->schedule));

  return 0;
}

int discrete_pumping_handler(program_context *p_ctx, char* data){
  char *post_ptr = data;
  uint8_t i = 0;
  for(; data[i] != 0 && data[i] != ','; i++){} //finds the first comma in the string

  data[i++] = 0; //set the comma to a 0 and increment postfix
  post_ptr = &data[i]; //ptr now points to first char in substr after comma

  //BEWARE OF TRUNCATION: ULONG >= 32bits, period_s is 16 bits
  uint16_t new_period = (uint16_t)strtol(post_ptr, NULL, 10);
  float mls_to_dose = strtof(data, NULL);
  uint32_t steps = (uint32_t)(mls_to_dose*p_ctx->pump_step_data->steps_per_ml);

  //checks if the expected dosing time is longer than the period between doses
  //WARNING: this is only calculated at setting time, so changing steps_per_ml (like during
  //autocal) would change this and potentially invalidate the condition in rare cases
  if(new_period < 1
     //|| (steps/DEFAULT_STEPS_PER_SEC()) > new_period)
     )
    return BLE_ATT_ERR_VALUE_NOT_ALLOWED;

  p_ctx->schedule->ml_per_dose = mls_to_dose;
  p_ctx->schedule->period_s = new_period;
  p_ctx->schedule->mode = INTERVAL;

  ESP_LOGI("ADN_STEPPER", "dosing period: %d", p_ctx->schedule->period_s);

  //commit new schedule to NVS
  ESP_ERROR_CHECK(store_sched(p_ctx->schedule));

  return 0;

}


/*
  byte 0 = hour
  byte 1 = minute
  byte 2-3 = amount * 100

 */
int timed_dosing_write(struct ble_gatt_access_ctxt *ctx, void *args){
  program_context *p_ctx = (program_context *)args;
  uint8_t data[32];
  uint16_t len = OS_MBUF_PKTLEN(ctx->om);
  uint8_t entry_end_idx = 6;

  //valid packets are always a multiple of 4 bytes
  //as each entry is 4 bytes exactly
  if(len > sizeof(data) || len % 4 != 0)
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

  ble_hs_mbuf_to_flat(ctx->om, data, sizeof(data), NULL);

  p_ctx->schedule->timed.count = 0; //reset array, whole sched gets tx every time

  for(uint8_t i = 0; i < len; i+=4){
    uint16_t amount_mult = *((uint16_t *)&data[2+i]);
    ESP_LOGI("ADN_BLE", "%2d:%2d  %d", data[0+i], data[1+i], amount_mult);
    p_ctx->schedule->timed.times[p_ctx->schedule->timed.count++] = (dose_time_data_t){
      .hour = data[0+i],
      .minute = data[1+i],
      .ml = ((float)amount_mult/100)
    };
    ESP_LOGI("ADN_BLE", "amount mult: %d", amount_mult);
    ESP_LOGI("ADN_BLE", "as stored: %f", ((float)amount_mult/100));
  }

  ESP_ERROR_CHECK(store_sched(p_ctx->schedule));
  p_ctx->schedule->mode = TIME;
  return 0;
}

int timed_dosing_read(struct ble_gatt_access_ctxt *ctx, void *args){
  program_context *p_ctx = (program_context *)args;
  char data[64];
  int len = 0;

  for(uint8_t i = 0; i < p_ctx->schedule->timed.count; i++){
    uint16_t amount_mult = (uint16_t)(p_ctx->schedule->timed.times[i].ml*100);
    ESP_LOGI("ADN_BLE", "amount mult: %d", amount_mult);
    ESP_LOGI("ADN_BLE", "as stored: %f", p_ctx->schedule->timed.times[i].ml);
    data[0+(i*4)] = p_ctx->schedule->timed.times[i].hour;
    data[1+(i*4)] = p_ctx->schedule->timed.times[i].minute;
    data[2+(i*4)] = amount_mult<<8;
    data[3+(i*4)] = amount_mult&0xFF;
    len+=4;
  }

  ESP_LOGI("ADN-BLE", "total entries: %d", (uint8_t)len/4);

  return os_mbuf_append(ctx->om, data, len) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

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

  //turn off cont pumping regardless of which mode
  p_ctx->hardware_states &= ~(PC_PUMP_CONTINUOUS);

  switch(data[0]){
  case 'c':
    return continuous_pumping_handler(p_ctx, data);
    break;
  default:
    return discrete_pumping_handler(p_ctx, data);
    break;

  }

  return 0;

}

int read_schedule(struct ble_gatt_access_ctxt *ctx, void* args){
  program_context *p_ctx = (program_context *)args;
  char data[32];
  int len;

  if(p_ctx->schedule->mode == INTERVAL)
    len = snprintf(data, 32, "%.2f,%ld", p_ctx->schedule->ml_per_dose, p_ctx->schedule->period_s);
  else
    len = snprintf(data, 32, "t%.2f,%ld", p_ctx->schedule->ml_per_dose, p_ctx->schedule->period_s);

  return os_mbuf_append(ctx->om, data, len) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}



int device_information(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args){

  switch (ctx->op) {

    //SOFTWARE AND BOARD VERSION
  case BLE_GATT_ACCESS_OP_READ_CHR:
    char data[32];
    data[0] = SOFTWARE_VERSION.v[0];
    data[1] = SOFTWARE_VERSION.v[1];
    data[2] = SOFTWARE_VERSION.v[2];
    data[3] = BOARD_VERSION.v[0];
    data[4] = BOARD_VERSION.v[1];
    data[5] = BOARD_VERSION.v[2];
    data[6] = BATCH_NUMBER.v[0];
    data[7] = BATCH_NUMBER.v[1];
    data[8] = BATCH_NUMBER.v[2];

    return os_mbuf_append(ctx->om, data, sizeof(version)*3) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    break;

    //SET SYS TIME
  case BLE_GATT_ACCESS_OP_WRITE_CHR:
    ;
    program_context *p_ctx = (program_context *)args;
    uint16_t len = OS_MBUF_PKTLEN(ctx->om);
    int64_t timestamp;

    if(len != sizeof(timestamp)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    int rc = os_mbuf_copydata(ctx->om, 0, sizeof(timestamp), &timestamp);
    if (rc != 0) return BLE_ATT_ERR_UNLIKELY;

    time_t unix_time = (time_t)timestamp;
    update_sys_time_from_long(unix_time);
    print_time(p_ctx);
    store_systime(NULL);
    return 0;
    break;

  default:
    return BLE_ATT_ERR_UNLIKELY;
  }
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

  ESP_LOGI("ADN_BLE", "raw data got: %s", data);

  mls = strtof((char *)data, NULL);

  if(mls == 0 || mls == ERANGE)
    return BLE_ATT_ERR_VALUE_NOT_ALLOWED;

  pump(mls, p_ctx);
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


int timezone_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void *args){

  program_context *p_ctx = (program_context *)args;
  uint16_t len = OS_MBUF_PKTLEN(ctx->om);
  char temp[10];

  if(len != 6 && len != 5) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

  ble_hs_mbuf_to_flat(ctx->om, temp, sizeof(temp), NULL);
  temp[len] = 0;

  //ESP_LOGI("ADN-BLE", "timezone recieved: %s", temp);
  strcpy(p_ctx->tz, temp);

  store_tz(p_ctx->tz);
  print_time(p_ctx);
  return 0;

}

int timed_schedule_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void *args){

  switch (ctx->op) {

  case BLE_GATT_ACCESS_OP_READ_CHR:
    return timed_dosing_read(ctx, args);
    break;

  case BLE_GATT_ACCESS_OP_WRITE_CHR:
    return timed_dosing_write(ctx, args);
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


int new_ble_dev_name(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args){
  program_context *p_ctx = (program_context *)args;
  uint16_t len = OS_MBUF_PKTLEN(ctx->om);
  char data[BLE_MAX_DEVNAME_LEN-1];

  if(len > sizeof(data)){
    ESP_LOGE("BLE", "PACKET_SIZE_WRONG");
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  }

  ble_hs_mbuf_to_flat(ctx->om, data, sizeof(data), NULL);
  data[len] = 0;

  ESP_ERROR_CHECK(store_device_name((void *)data));
  strcpy(p_ctx->BLE_device_name, data);


  return 0;
}


int auto_cal_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void* args){
  program_context *p_ctx = (program_context *)args;
  uint16_t len = OS_MBUF_PKTLEN(ctx->om);
  char data[9]; //this allows for 10.xxxxx precision
  float actual_output;

  if(len > sizeof(data)){
    ESP_LOGE("BLE", "PACKET_SIZE_WRONG");
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  }

  ble_hs_mbuf_to_flat(ctx->om, data, sizeof(data), NULL);
  data[len] = 0;

  actual_output = strtof(data, NULL);

  float calibration_correction_mult = 1+((10 - actual_output)/10);
  p_ctx->pump_step_data->steps_per_ml *= calibration_correction_mult;

  ESP_LOGI("AD_BLE", "multiplier: %f, new_val %d", calibration_correction_mult, p_ctx->pump_step_data->steps_per_ml);

  ESP_ERROR_CHECK(store_sched(p_ctx->schedule));

  return 0;
}



int write_ota_command(struct ble_gatt_access_ctxt *ctx, void* args){
  program_context *p_ctx = (program_context *)args;
  uint16_t len = OS_MBUF_PKTLEN(ctx->om);
  uint8_t packet[OTA_PACKET_MAX];
  uint32_t offset; //either image size for start, or data length for write chunk
  if (len == 0 || len > OTA_PACKET_MAX)
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

  int rc = os_mbuf_copydata(ctx->om, 0, len, packet);

  if (rc != 0)
    return BLE_ATT_ERR_UNLIKELY;

  switch(packet[0]){
  case OTA_CMD_START:
    if(len != 5)
      return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    memcpy(&offset, &packet[1], sizeof(offset));
    ota_start(offset);
    break;
  case OTA_CMD_DATA:
    memcpy(&offset, &packet[1], sizeof(offset));
    ota_write_chunk(offset, &packet[5], len - 5);
    break;
  case OTA_CMD_FINISH:
    ota_finish();
    break;
  case OTA_CMD_ABORT:
    break;
  default:
    return BLE_ATT_ERR_UNLIKELY;
    break;
  }
  return 0;
}


int ota_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctx, void *args){

  switch (ctx->op) {
  case BLE_GATT_ACCESS_OP_WRITE_CHR:
    return write_ota_command(ctx, args);
    break;
  default:
    return BLE_ATT_ERR_UNLIKELY;
    break;
  }

}
