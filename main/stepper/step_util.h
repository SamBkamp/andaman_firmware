#ifndef STEP_UTIL_H
#define STEP_UTIL_H

#include "prot.h"

#define TIMER_2MHZ_RES 1 * 1000 * 1000 * 2
#define PUMP_MIN_RATE 20
#define PUMP_MAX_RATE 500
#define DEFAULT_DISCRETE_SPEED 220 //this is a magic number, sorry. The lower this number is, the faster
#define DEFAULT_STEPS_PER_SEC() TIMER_2MHZ_RES/(DEFAULT_DISCRETE_SPEED * 2)


void timer_init_start (program_context *p_ctx, uint32_t alarm_count);
void pump(float ml, program_context *p_ctx);
void pump_continuous(float ml_per_min, program_context *p_ctx);

#endif
