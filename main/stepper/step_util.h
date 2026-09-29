#ifndef STEP_UTIL_H
#define STEP_UTIL_H

#include "prot.h"

void timer_init_start (program_context *p_ctx, uint32_t alarm_count);
void pump(float ml, program_context *p_ctx);
void pump_continuous(float ml_per_min, program_context *p_ctx);

#endif
