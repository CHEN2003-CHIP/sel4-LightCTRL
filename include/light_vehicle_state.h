#ifndef LIGHT_VEHICLE_STATE_H
#define LIGHT_VEHICLE_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "light_protocol.h"

#define LIGHT_VEHICLE_SPEED_STEP_KPH  10U
#define LIGHT_VEHICLE_SPEED_MAX_KPH   180U
#define LIGHT_VEHICLE_GEAR_MAX        LIGHT_VEHICLE_GEAR_DRIVE
#define LIGHT_VEHICLE_AMBIENT_MAX     LIGHT_VEHICLE_AMBIENT_NIGHT
#define LIGHT_VEHICLE_DRIVE_MODE_MAX  LIGHT_VEHICLE_DRIVE_MODE_EMERGENCY

typedef enum {
    LIGHT_VEHICLE_STATE_REASON_OK = 0,
    LIGHT_VEHICLE_STATE_REASON_INVALID_REQUEST = 1,
    LIGHT_VEHICLE_STATE_REASON_INVALID_VALUE = 2,
} light_vehicle_state_reason_t;

typedef struct {
    light_vehicle_state_request_t request;
    light_vehicle_state_t next_state;
    bool accepted;
    bool changed;
    light_vehicle_state_reason_t reason;
} light_vehicle_state_update_result_t;

light_vehicle_state_update_result_t light_vehicle_state_apply_request(light_vehicle_state_t state,
                                                                      light_vehicle_state_request_t request);
const char *light_vehicle_gear_name(uint8_t gear);
const char *light_vehicle_ambient_light_name(uint8_t ambient_light);
const char *light_vehicle_drive_mode_name(uint8_t drive_mode);

#endif
