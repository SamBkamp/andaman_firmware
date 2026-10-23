#include <stdbool.h>
#include <stdint.h>
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "ble_ota";

static esp_ota_handle_t ota_handle;
static const esp_partition_t *ota_partition;
static uint32_t ota_image_size;
static uint32_t ota_received;
static bool ota_in_progress;

esp_err_t ota_start(uint32_t image_size){
  if (ota_in_progress || image_size == 0)
    return ESP_ERR_INVALID_STATE;

  ota_partition = esp_ota_get_next_update_partition(NULL);

  if (ota_partition == NULL || image_size > ota_partition->size)
    return ESP_ERR_INVALID_SIZE;


  esp_err_t err = esp_ota_begin(ota_partition, image_size, &ota_handle);

  if (err != ESP_OK)
    return err;


  ota_image_size = image_size;
  ota_received = 0;
  ota_in_progress = true;

  ESP_LOGI(TAG, "OTA started: %lu bytes, partition %s", image_size, ota_partition->label);

  return ESP_OK;
}

esp_err_t ota_write_chunk(uint32_t offset, const uint8_t *data, size_t len){
  if (!ota_in_progress || data == NULL || len == 0)
    return ESP_ERR_INVALID_STATE;

  if (offset != ota_received || len > ota_image_size - ota_received)
      return ESP_ERR_INVALID_SIZE;

  ESP_LOGI(TAG, "Writing chunk at offset %x", offset);

  esp_err_t err = esp_ota_write(ota_handle, data, len);

  if (err != ESP_OK) {
    esp_ota_abort(ota_handle);
    ota_in_progress = false;
    return err;
  }

  ota_received += len;
  return ESP_OK;
}

esp_err_t ota_finish(void){
  if (!ota_in_progress || ota_received != ota_image_size)
    return ESP_ERR_INVALID_STATE;


  // esp_ota_end() validates the complete image and
  // releases the OTA handle, even if validation fails.
  ota_in_progress = false;
  esp_err_t err = esp_ota_end(ota_handle);

  if (err != ESP_OK)
    return err;

  err = esp_ota_set_boot_partition(ota_partition);

  if (err != ESP_OK)
    return err;

  ESP_LOGI(TAG, "OTA complete; restarting");
  esp_restart();

  return ESP_OK; // Normally never reached.
}
