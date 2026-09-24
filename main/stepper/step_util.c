#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_err.h"
#include "esp_log.h"

#include "stepper/step_util.h"
#include "prot.h"

static bool pump_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx){
  step_struct *ss = (step_struct *)user_ctx;
  BaseType_t woken = pdFALSE;
  ss->state^=1;
  gpio_set_level(PIN_STEP, ss->state);
  ss->steps_achieved+=ss->state;

  /* ESP_DRAM_LOGI("erm", */
  /*               "state=%d gpio=%d", */
  /*               ss->state, */
  /*               gpio_get_level(PIN_STEP) */
  /*               ); */
  if(ss->steps_achieved >= ss->total_steps) {
    gptimer_stop(timer);
    vTaskNotifyGiveFromISR(ss->callback_task, &woken);
  }

  gpio_set_level(PIN_LED_ERROR, gpio_get_level(PIN_FAULTB) ^ 1);
  //pin_faultb is active low, so we invert it - LED will only be on when fault is low


  return woken == pdTRUE;
}

void timer_init_start (step_struct *user_data){
  gptimer_config_t timer_config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT, // Select the default clock source
    .direction = GPTIMER_COUNT_UP,      // Counting direction is up
    .resolution_hz = 1 * 1000 * 1000 * 2,   // resolution for the timer
  };

  ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &user_data->gptimer));


  gptimer_alarm_config_t alarm_config = {
    .reload_count = 0,      // on alarm, reset counter to 0
    .alarm_count = 370/2, // 370 used to turn the 1mhz frequency to us, but now the freq is different so this is just kinda.. here
    .flags.auto_reload_on_alarm = true, // Enable auto-reload function
  };

  ESP_ERROR_CHECK(gptimer_set_alarm_action(user_data->gptimer, &alarm_config));

  gptimer_event_callbacks_t cbs = {.on_alarm = pump_alarm};

  ESP_ERROR_CHECK(gptimer_register_event_callbacks(user_data->gptimer, &cbs, user_data));

  ESP_ERROR_CHECK(gptimer_enable(user_data->gptimer));

}



void deregister_pump(void *arg){
  step_struct *ss = (step_struct *)arg;
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);//wait for timer isr to finish
  sleep_driver();
  ESP_LOGI("DOSER", "driver sleep");

  vTaskDelete(NULL);
}



void pump(float ml, step_struct *pump_step_data){
  pump_step_data->total_steps = (uint32_t)(ml*pump_step_data->steps_per_ml);
  pump_step_data->steps_achieved = 0;
  pump_step_data->state = 0;
  wake_driver();
  ESP_LOGI("DOSER", "driver awake");
  if(pump_step_data->gptimer == NULL){//timer isn't initialised
    ESP_LOGI("DOSER", "timer not initisalised, initialising...");
    timer_init_start(pump_step_data);
  }

  xTaskCreate(deregister_pump, "pump_completed", 2048,
              pump_step_data, 5, &pump_step_data->callback_task);

  //pump_step_data->callback_task = xTaskGetCurrentTaskHandle();
  ESP_ERROR_CHECK(gptimer_start(pump_step_data->gptimer));


}
