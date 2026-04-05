#ifndef LIGHT_RUNTIME_GUARD_H
#define LIGHT_RUNTIME_GUARD_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t vehicle_speed;
    uint8_t last_turn_state;
    uint8_t last_beam_state;
    uint8_t last_brake_state;
    uint8_t last_position_state;
} light_runtime_guard_context_t;

typedef struct {
    bool allowed;
    bool report_fault;
    uint8_t error_code;
} light_runtime_guard_result_t;

light_runtime_guard_result_t light_runtime_guard_check_action(uint32_t action_channel,
                                                             light_runtime_guard_context_t context);

#endif
