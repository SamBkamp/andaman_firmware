#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_err.h"
#include "esp_log.h"

#include "stepper/step_util.h"
#include "prot.h"


static bool pump_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *ctx){
  program_context *p_ctx = (program_context *)ctx;
  BaseType_t woken = pdFALSE;
  p_ctx->pump_step_data->state^=1;
  gpio_set_level(PIN_STEP, p_ctx->pump_step_data->state);
  p_ctx->pump_step_data->steps_achieved+=p_ctx->pump_step_data->state;

  /* ESP_DRAM_LOGI("erm", */
  /*               ); */

  if(p_ctx->pump_step_data->steps_achieved >= p_ctx->pump_step_data->total_steps
     && (p_ctx->hardware_states & PC_PUMP_CONTINUOUS) == 0) {
    gptimer_stop(timer);
    vTaskNotifyGiveFromISR(p_ctx->pump_step_data->callback_task, &woken);
  }

  gpio_set_level(PIN_LED_ERROR, gpio_get_level(PIN_FAULTB) ^ 1);
  //pin_faultb is active low, so we invert it - LED will only be on when fault is low
  switch(gpio_get_level(PIN_FAULTB)){
  case 0:
    ESP_LOGW("ADN_PUMP", "PUMP DRIVER FAULT");
    gptimer_stop(timer);
    vTaskNotifyGiveFromISR(p_ctx->pump_step_data->callback_task, &woken);
    break;
  default:
    break;
  }

  return woken == pdTRUE;
}

void timer_init_start (program_context *p_ctx, uint32_t alarm_count){
  gptimer_config_t timer_config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT, // Select the default clock source
    .direction = GPTIMER_COUNT_UP,      // Counting direction is up
    .resolution_hz = TIMER_2MHZ_RES,
  };

  ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &p_ctx->pump_step_data->gptimer));


  gptimer_alarm_config_t alarm_config = {
    .reload_count = 0,      // on alarm, reset counter to 0
    .alarm_count = alarm_count,
    .flags.auto_reload_on_alarm = true, // Enable auto-reload function
  };

  ESP_ERROR_CHECK(gptimer_set_alarm_action(p_ctx->pump_step_data->gptimer, &alarm_config));

  gptimer_event_callbacks_t cbs = {.on_alarm = pump_alarm};

  ESP_ERROR_CHECK(gptimer_register_event_callbacks(p_ctx->pump_step_data->gptimer, &cbs, p_ctx));

  ESP_ERROR_CHECK(gptimer_enable(p_ctx->pump_step_data->gptimer));

}



void deregister_pump(void *arg){
  program_context *p_ctx = (program_context  *)arg;
  //step_struct *ss = (step_struct *)arg;
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);//wait for timer isr to finish
  sleep_driver();
  ESP_LOGI("DOSER", "driver sleep");
  p_ctx->pump_step_data->total_steps = 0; //reset total steps when done
  p_ctx->hardware_states &= ~(PC_PUMP_ACTIVE);
  vTaskDelete(NULL);
}



void pump(float ml, program_context *p_ctx){
  if((p_ctx->hardware_states & PC_PUMP_ACTIVE) != 0) return; //pumping in progress
  uint32_t steps = (uint32_t)(ml*p_ctx->pump_step_data->steps_per_ml);

  p_ctx->pump_step_data->total_steps = steps;
  p_ctx->pump_step_data->steps_achieved = 0;
  p_ctx->pump_step_data->state = 0;
  wake_driver();
  ESP_LOGI("DOSER", "driver awake");

  if(p_ctx->pump_step_data->gptimer == NULL){//timer isn't initialised
    ESP_LOGI("DOSER", "timer not initisalised, initialising...");
    timer_init_start(p_ctx, DEFAULT_DISCRETE_SPEED);
  }else{
    //reset alarm config to make sure discrete
    //pumping is consistent
    gptimer_alarm_config_t alarm_config = {
      .reload_count = 0,      // on alarm, reset counter to 0
      .alarm_count = DEFAULT_DISCRETE_SPEED,
      .flags.auto_reload_on_alarm = true, // Enable auto-reload function
    };

    ESP_ERROR_CHECK(gptimer_set_alarm_action(p_ctx->pump_step_data->gptimer, &alarm_config));

  }

  xTaskCreate(deregister_pump, "pump_completed", 2048,
              p_ctx, 5, &p_ctx->pump_step_data->callback_task);

  p_ctx->hardware_states |= PC_PUMP_ACTIVE;
  p_ctx->hardware_states &= ~(PC_PUMP_CONTINUOUS); //turn off cont. flag. This function only for discrete


  ESP_ERROR_CHECK(gptimer_start(p_ctx->pump_step_data->gptimer));
}


void pump_continuous(float ml_per_min, program_context *p_ctx){
  if(ml_per_min <= 0) return;

  //TODO: add a timeout here so it doesn't spin indefinetly
  while((p_ctx->hardware_states & PC_PUMP_ACTIVE) != 0){ //spin until the pump becomes free?
    ESP_LOGW("ADN_PUMP", "PUMP STILL ACTIVE - WAITING");
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  uint32_t alarm_count = (uint32_t)(TIMER_2MHZ_RES /
                                    ((ml_per_min / 60.0f) *
                                     p_ctx->pump_step_data->steps_per_ml * 2));
  //two times as fast due to each alarm call only performing half of the square wave

  if(alarm_count < PUMP_MIN_RATE || alarm_count > PUMP_MAX_RATE){
    ESP_LOGW("ADN_PUMP", "INVALID PUMP RATE: %d", alarm_count);
    return;
  }

  if(p_ctx->pump_step_data->gptimer == NULL){//timer isn't initialised
    ESP_LOGI("DOSER", "timer not initisalised, initialising...");
    timer_init_start(p_ctx, alarm_count);
  }else {
    gptimer_alarm_config_t alarm_config = {
      .reload_count = 0,      // on alarm, reset counter to 0
      .alarm_count = alarm_count,
      .flags.auto_reload_on_alarm = true, // Enable auto-reload function
    };

    ESP_ERROR_CHECK(gptimer_set_alarm_action(p_ctx->pump_step_data->gptimer, &alarm_config));
  }

  p_ctx->hardware_states |= PC_PUMP_CONTINUOUS;
  p_ctx->hardware_states |= PC_PUMP_ACTIVE;
  gpio_set_level(PIN_LOWI_MODE, 1); //set lowi mode for continuous dosing


  wake_driver();
  ESP_LOGI("DOSER", "driver awake");

  xTaskCreate(deregister_pump, "pump_completed", 2048,
              p_ctx, 5, &p_ctx->pump_step_data->callback_task);

  ESP_ERROR_CHECK(gptimer_start(p_ctx->pump_step_data->gptimer));
}
