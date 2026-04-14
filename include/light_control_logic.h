#ifndef LIGHT_CONTROL_LOGIC_H
#define LIGHT_CONTROL_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#include "light_fault_mode.h"
#include "light_protocol.h"

typedef enum {
    LIGHT_CONTROL_REASON_OK = 0,
    LIGHT_CONTROL_REASON_INVALID_CMD = 1,
} light_control_reason_t;

typedef struct {
    uint8_t cmd;
    light_operator_request_t next_request;
    bool accepted;
    bool notify;
    light_control_reason_t reason;
} light_control_command_result_t;

light_control_command_result_t light_control_apply_operator_command(light_operator_request_t request,
                                                                    uint8_t cmd);
light_target_output_t light_control_compute_target_output(light_operator_request_t request,
                                                          light_vehicle_state_t vehicle_state,
                                                          fault_mode_t fault_mode);

#endif
