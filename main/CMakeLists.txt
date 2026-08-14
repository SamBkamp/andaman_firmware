# Registers the component and sets its source files and include directories
idf_component_register(
SRCS "stepper/step_util.c" "wifi/wifi_driver.c" "ble/ble_cb.c" "ble/ble_driver.c" "generic_util.c" "main.c" 
INCLUDE_DIRS "."
REQUIRES  driver esp_driver_gpio esp_driver_gptimer lwip esp_wifi nvs_flash bt
)
