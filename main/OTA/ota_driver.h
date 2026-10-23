#ifndef OTA_DRIVER

#define OTA_CMD_START 0
#define OTA_CMD_DATA 1
#define OTA_CMD_FINISH 2
#define OTA_CMD_ABORT 3
#define OTA_PACKET_MAX 258

esp_err_t ota_start(uint32_t image_size);
esp_err_t ota_write_chunk(uint32_t offset, const uint8_t *data, size_t len);
esp_err_t ota_finish(void);


#endif
