#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_err.h"
#include "esp_log.h"

#include "stepper/step_util.h"
#include "prot.h"

#define TIMER_2MHZ_RES 1 * 1000 * 1000 * 2
#define PUMP_MIN_RATE 20
#define PUMP_MAX_RATE 110

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

void timer_init_start (step_struct *user_data, uint32_t alarm_count){
  gptimer_config_t timer_config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT, // Select the default clock source
    .direction = GPTIMER_COUNT_UP,      // Counting direction is up
    .resolution_hz = TIMER_2MHZ_RES,
  };

  ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &user_data->gptimer));


  gptimer_alarm_config_t alarm_config = {
    .reload_count = 0,      // on alarm, reset counter to 0
    .alarm_count = alarm_count,
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
  ss->total_steps = 0; //reset total steps when done
  vTaskDelete(NULL);
}



void pump(float ml, program_context *p_ctx){
  if(p_ctx->pump_step_data->total_steps != 0) return; //pumping in progress

  p_ctx->pump_step_data->total_steps = (uint32_t)(ml*p_ctx->pump_step_data->steps_per_ml);
  p_ctx->pump_step_data->steps_achieved = 0;
  p_ctx->pump_step_data->state = 0;
  wake_driver();
  ESP_LOGI("DOSER", "driver awake");
  if(p_ctx->pump_step_data->gptimer == NULL){//timer isn't initialised
    ESP_LOGI("DOSER", "timer not initisalised, initialising...");
    timer_init_start(p_ctx->pump_step_data, 370/2); //370/2 is a magic number, sorry
  }

  xTaskCreate(deregister_pump, "pump_completed", 2048,
              p_ctx->pump_step_data, 5, &p_ctx->pump_step_data->callback_task);

  //pump_step_data->callback_task = xTaskGetCurrentTaskHandle();
  ESP_ERROR_CHECK(gptimer_start(p_ctx->pump_step_data->gptimer));


}


void pump_continuous(float ml_per_min, step_struct *pump_step_data){
  if(ml_per_min <= 0) return;
  uint32_t alarm_count = (uint32_t)((TIMER_2MHZ_RES * 2.0f) /
                                    (ml_per_min / 60.0f *
                                     pump_step_data->steps_per_ml));
  //two times as fast due to each alarm call only performing half of the square wave

  if(alarm_count < PUMP_MIN_RATE || alarm_count > PUMP_MAX_RATE) return;

  if(pump_step_data->gptimer == NULL){//timer isn't initialised
    ESP_LOGI("DOSER", "timer not initisalised, initialising...");
    timer_init_start(pump_step_data, alarm_count);
  }else {
    gptimer_alarm_config_t alarm_config = {
      .reload_count = 0,      // on alarm, reset counter to 0
      .alarm_count = alarm_count,
      .flags.auto_reload_on_alarm = true, // Enable auto-reload function
    };

    ESP_ERROR_CHECK(gptimer_set_alarm_action(pump_step_data->gptimer, &alarm_config));
  }



}
