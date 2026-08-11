# Registers the component and sets its source files and include directories
idf_component_register(
SRCS "main.c" "step_util.c"
INCLUDE_DIRS "."
REQUIRES  driver esp_driver_gpio esp_driver_gptimer
)
