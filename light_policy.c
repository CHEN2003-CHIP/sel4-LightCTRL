#include "light_policy.h"

light_policy_state_t light_policy_init_state(void) {
    light_policy_state_t state;

    state.allow_flags = LIGHT_ALLOW_POSITION;
    state.vehicle_speed = 10;

    return state;
}

light_policy_result_t light_policy_apply_command(light_policy_state_t state, uint8_t cmd) {
    light_policy_result_t result;

    result.cmd = cmd;
    result.prev_allow_flags = state.allow_flags;
    result.next_allow_flags = state.allow_flags;
    result.accepted = false;
    result.notify = false;
    result.reason = LIGHT_POLICY_OK;

    switch (cmd) {
        case LIGHT_CMD_LOW_BEAM_ON:
            result.next_allow_flags |= LIGHT_ALLOW_LOW_BEAM;
            result.accepted = true;
            result.notify = true;
            break;

        case LIGHT_CMD_LOW_BEAM_OFF:
            result.next_allow_flags &= ~LIGHT_ALLOW_LOW_BEAM;
            result.next_allow_flags &= ~LIGHT_ALLOW_HIGH_BEAM;
            result.accepted = true;
            result.notify = true;
            break;

        case LIGHT_CMD_HIGH_BEAM_ON:
            if ((state.allow_flags & LIGHT_ALLOW_LOW_BEAM) == 0U) {
                result.reason = LIGHT_POLICY_REJECT_LOW_BEAM_REQUIRED;
                break;
            }
            if ((state.allow_flags & LIGHT_ALLOW_BRAKE) != 0U) {
                result.reason = LIGHT_POLICY_REJECT_BRAKE_ACTIVE;
                break;
            }
            result.next_allow_flags |= LIGHT_ALLOW_HIGH_BEAM;
            result.accepted = true;
            result.notify = true;
            break;

        case LIGHT_CMD_HIGH_BEAM_OFF:
            result.next_allow_flags &= ~LIGHT_ALLOW_HIGH_BEAM;
            result.accepted = true;
            result.notify = true;
            break;

        case LIGHT_CMD_LEFT_TURN_ON:
            if ((state.allow_flags & LIGHT_ALLOW_BRAKE) != 0U) {
                result.reason = LIGHT_POLICY_REJECT_BRAKE_ACTIVE;
                break;
            }
            if ((state.allow_flags & LIGHT_ALLOW_TURN_RIGHT) != 0U) {
                result.reason = LIGHT_POLICY_REJECT_RIGHT_TURN_ACTIVE;
                break;
            }
            result.next_allow_flags |= LIGHT_ALLOW_TURN_LEFT;
            result.accepted = true;
            result.notify = true;
            break;

        case LIGHT_CMD_LEFT_TURN_OFF:
            result.next_allow_flags &= ~LIGHT_ALLOW_TURN_LEFT;
            result.accepted = true;
            result.notify = true;
            break;

        case LIGHT_CMD_RIGHT_TURN_ON:
            if ((state.allow_flags & LIGHT_ALLOW_BRAKE) != 0U) {
                result.reason = LIGHT_POLICY_REJECT_BRAKE_ACTIVE;
                break;
            }
            if ((state.allow_flags & LIGHT_ALLOW_TURN_LEFT) != 0U) {
                result.reason = LIGHT_POLICY_REJECT_LEFT_TURN_ACTIVE;
                break;
            }
            result.next_allow_flags |= LIGHT_ALLOW_TURN_RIGHT;
            result.accepted = true;
            result.notify = true;
            break;

        case LIGHT_CMD_RIGHT_TURN_OFF:
            result.next_allow_flags &= ~LIGHT_ALLOW_TURN_RIGHT;
            result.accepted = true;
            result.notify = true;
            break;

        case LIGHT_CMD_POSITION_ON:
            result.next_allow_flags |= LIGHT_ALLOW_POSITION;
            result.accepted = true;
            result.notify = true;
            break;

        case LIGHT_CMD_POSITION_OFF:
            result.next_allow_flags &= ~LIGHT_ALLOW_POSITION;
            result.accepted = true;
            result.notify = true;
            break;

        case LIGHT_CMD_BRAKE_ON:
            result.next_allow_flags |= LIGHT_ALLOW_BRAKE;
            result.accepted = true;
            result.notify = true;
            break;

        case LIGHT_CMD_BRAKE_OFF:
            result.next_allow_flags &= ~LIGHT_ALLOW_BRAKE;
            result.accepted = true;
            result.notify = true;
            break;

        default:
            result.reason = LIGHT_POLICY_REJECT_INVALID_CMD;
            break;
    }

    return result;
}

light_target_state_t light_policy_target_from_flags(uint32_t allow_flags) {
    light_target_state_t target;

    target.brake = (allow_flags & LIGHT_ALLOW_BRAKE) != 0U;
    target.turn_left = false;
    target.turn_right = false;
    target.low_beam = false;
    target.high_beam = false;
    target.position = (allow_flags & LIGHT_ALLOW_POSITION) != 0U;

    if ((allow_flags & LIGHT_ALLOW_TURN_LEFT) != 0U) {
        target.turn_left = true;
    } else if ((allow_flags & LIGHT_ALLOW_TURN_RIGHT) != 0U) {
        target.turn_right = true;
    }

    if ((allow_flags & LIGHT_ALLOW_HIGH_BEAM) != 0U) {
        target.high_beam = true;
    } else if ((allow_flags & LIGHT_ALLOW_LOW_BEAM) != 0U) {
        target.low_beam = true;
    }

    return target;
}
