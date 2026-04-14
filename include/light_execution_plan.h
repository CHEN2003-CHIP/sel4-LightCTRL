#ifndef LIGHT_EXECUTION_PLAN_H
#define LIGHT_EXECUTION_PLAN_H

#include <stddef.h>
#include <stdint.h>

#include "light_protocol.h"

#define LIGHT_EXECUTION_PLAN_MAX_ACTIONS 8U

typedef struct {
    uint8_t turn_state;
    uint8_t beam_state;
    uint8_t brake_state;
    uint8_t position_state;
} light_execution_state_t;

typedef struct {
    uint32_t actions[LIGHT_EXECUTION_PLAN_MAX_ACTIONS];
    size_t action_count;
    light_execution_state_t next_state;
} light_execution_plan_t;

light_execution_state_t light_execution_state_init(void);
light_execution_plan_t light_execution_plan_build(light_execution_state_t current_state,
                                                  light_target_output_t target_output);

#endif
