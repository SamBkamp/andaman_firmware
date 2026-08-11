#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_err.h"
#include "esp_log.h"

#include "step_util.h"
#include "prot.h"

static bool pump_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx){
  step_struct *ss = (step_struct *)user_ctx;
  BaseType_t woken = pdFALSE;
  ss->state^=1;
  gpio_set_level(PIN_STEP, ss->state);
  ss->steps_achieved+=ss->state;

  if(ss->steps_achieved >= ss->total_steps) {
    gptimer_stop(timer);
    //gptimer_del_timer(timer);
    vTaskNotifyGiveFromISR(ss->callback_task, &woken);
    free(ss);
  }

  return woken == pdTRUE;
}

void timer_init_start (step_struct *user_data){
  gptimer_handle_t gptimer = NULL;
  gptimer_config_t timer_config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT, // Select the default clock source
    .direction = GPTIMER_COUNT_UP,      // Counting direction is up
    .resolution_hz = 1 * 1000 * 1000,   // Resolution is 1 MHz, i.e., 1 tick equals 1 microsecond
  };

  ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

  gptimer_alarm_config_t alarm_config = {
    .reload_count = 0,      // on alarm, reset counter to 0
    .alarm_count = 370/2, // in us
    .flags.auto_reload_on_alarm = true, // Enable auto-reload function
  };

  ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

  gptimer_event_callbacks_t cbs = {.on_alarm = pump_alarm};

  ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, user_data));

  ESP_ERROR_CHECK(gptimer_enable(gptimer));
  ESP_ERROR_CHECK(gptimer_start(gptimer));

}


void pump(float ml){
  step_struct *pump_step_data = malloc(sizeof(step_struct));
  memset(pump_step_data, 0, sizeof(step_struct));
  pump_step_data->total_steps = 5000;
  pump_step_data->callback_task = xTaskGetCurrentTaskHandle();

  timer_init_start(pump_step_data);
}
