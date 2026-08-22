#include "nvs.h"
#include "nvs_flash.h"
#include "prot.h"
#include "nvs/nvs_driver.h"
#include "esp_err.h"

#define NVS_NAMESPACE "AD_DOSER_CFG"
#define NVS_KEY_SCHEDULE "doser_sched"
#define NVS_KEY_CALIBRATION "calibration"
#define NVS_KEY_HWSTATE "hw_state"


esp_err_t store_sched(doser_schedule *sched){
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);

  if(err != ESP_OK)
    return err;

  err = nvs_set_blob(handle, NVS_KEY_SCHEDULE, sched, sizeof(doser_schedule));
  if(err == ESP_OK)
    err = nvs_commit(handle);

  nvs_close(handle);

  return err;
}

esp_err_t load_schedule(doser_schedule *sched){
    nvs_handle_t handle;
    size_t size = sizeof(doser_schedule);

    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);

    if (err != ESP_OK)
      return err;

    err = nvs_get_blob(handle, NVS_KEY_SCHEDULE, sched, &size);
    nvs_close(handle);

    return err;
}

esp_err_t load_step_calibration(uint16_t *steps_per_ml){
  nvs_handle_t handle;
  size_t size = sizeof(steps_per_ml);

  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);

  if (err != ESP_OK)
    return err;

  err = nvs_get_blob(handle, NVS_KEY_CALIBRATION, steps_per_ml, &size);
  nvs_close(handle);

  return err;
}


esp_err_t store_step_calibration(uint16_t *steps_per_ml){
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);

  if(err != ESP_OK)
    return err;

  err = nvs_set_blob(handle, NVS_KEY_CALIBRATION, steps_per_ml, sizeof(steps_per_ml));
  if(err == ESP_OK)
    err = nvs_commit(handle);

  nvs_close(handle);

  return err;
}

esp_err_t load_hardware_state(uint8_t *hardware_state){
  nvs_handle_t handle;
  size_t size = sizeof(*hardware_state);

  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);

  if (err != ESP_OK)
    return err;

  err = nvs_get_blob(handle, NVS_KEY_HWSTATE, hardware_state, &size);
  nvs_close(handle);

  return err;

}


esp_err_t store_hardware_state(uint8_t *hardware_state){
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);

  if(err != ESP_OK)
    return err;

  err = nvs_set_blob(handle, NVS_KEY_HWSTATE, hardware_state, sizeof(*hardware_state));
  if(err == ESP_OK)
    err = nvs_commit(handle);

  nvs_close(handle);

  return err;
}
