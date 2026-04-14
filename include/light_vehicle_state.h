#ifndef LIGHT_VEHICLE_STATE_H
#define LIGHT_VEHICLE_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "light_protocol.h"

#define LIGHT_VEHICLE_SPEED_STEP_KPH  10U
#define LIGHT_VEHICLE_SPEED_MAX_KPH   180U

typedef enum {
    LIGHT_VEHICLE_STATE_REASON_OK = 0,
    LIGHT_VEHICLE_STATE_REASON_INVALID_CMD = 1,
} light_vehicle_state_reason_t;

typedef struct {
    uint8_t cmd;
    light_vehicle_state_t next_state;
    bool accepted;
    bool changed;
    light_vehicle_state_reason_t reason;
} light_vehicle_state_update_result_t;

light_vehicle_state_update_result_t light_vehicle_state_apply_command(light_vehicle_state_t state,
                                                                      uint8_t cmd);

#endif
