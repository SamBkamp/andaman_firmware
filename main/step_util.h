#ifndef STEP_UTIL_H
#define STEP_UTIL_H

#include "prot.h"

static bool pump_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);
void timer_init_start (step_struct *user_data);
void pump(float ml, step_struct *pump_step_data);


#endif
