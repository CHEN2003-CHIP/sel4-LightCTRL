#ifndef LIGHT_POLICY_H
#define LIGHT_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#include "light_protocol.h"

typedef enum {
    LIGHT_POLICY_OK = 0,
    LIGHT_POLICY_REJECT_INVALID_CMD = 1,
    LIGHT_POLICY_REJECT_LOW_BEAM_REQUIRED = 2,
    LIGHT_POLICY_REJECT_BRAKE_ACTIVE = 3,
    LIGHT_POLICY_REJECT_RIGHT_TURN_ACTIVE = 4,
    LIGHT_POLICY_REJECT_LEFT_TURN_ACTIVE = 5,
} light_policy_reason_t;

typedef struct {
    uint32_t allow_flags;
    uint16_t vehicle_speed;
} light_policy_state_t;

typedef struct {
    bool brake;
    bool turn_left;
    bool turn_right;
    bool low_beam;
    bool high_beam;
    bool position;
} light_target_state_t;

typedef struct {
    uint8_t cmd;
    uint32_t prev_allow_flags;
    uint32_t next_allow_flags;
    bool accepted;
    bool notify;
    light_policy_reason_t reason;
} light_policy_result_t;

light_policy_state_t light_policy_init_state(void);
light_policy_result_t light_policy_apply_command(light_policy_state_t state, uint8_t cmd);
light_target_state_t light_policy_target_from_flags(uint32_t allow_flags);

#endif
