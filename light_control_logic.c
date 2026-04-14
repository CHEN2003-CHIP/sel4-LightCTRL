#include "light_control_logic.h"

#include "light_output_policy.h"

static light_target_state_t requested_target_from_request(light_operator_request_t request,
                                                          light_vehicle_state_t vehicle_state) {
    light_target_state_t requested_target;
    bool ignition_on = vehicle_state.ignition_on != 0U;
    bool brake_active = request.brake_req != 0U || vehicle_state.brake_pedal != 0U;
    bool left_requested = request.left_turn_req != 0U;
    bool right_requested = request.right_turn_req != 0U;
    bool low_requested = request.low_beam_req != 0U;
    bool high_requested = request.high_beam_req != 0U;
    bool marker_requested = request.marker_req != 0U;

    requested_target.brake = brake_active;
    requested_target.turn_left = false;
    requested_target.turn_right = false;
    requested_target.low_beam = false;
    requested_target.high_beam = false;
    requested_target.position = marker_requested;

    if (!ignition_on) {
        return requested_target;
    }

    if (!brake_active && vehicle_state.speed_kph <= 120U) {
        if (left_requested && !right_requested) {
            requested_target.turn_left = true;
        } else if (right_requested && !left_requested) {
            requested_target.turn_right = true;
        }
    }

    if (low_requested) {
        requested_target.low_beam = true;
    }

    if (high_requested && low_requested && !brake_active && vehicle_state.speed_kph >= 10U) {
        requested_target.low_beam = false;
        requested_target.high_beam = true;
    }

    return requested_target;
}

static light_target_output_t target_output_from_state(light_target_state_t state) {
    light_target_output_t target_output;

    target_output.brake_on = state.brake ? 1U : 0U;
    target_output.left_turn_on = state.turn_left ? 1U : 0U;
    target_output.right_turn_on = state.turn_right ? 1U : 0U;
    target_output.low_beam_on = state.low_beam ? 1U : 0U;
    target_output.high_beam_on = state.high_beam ? 1U : 0U;
    target_output.marker_on = state.position ? 1U : 0U;

    return target_output;
}

light_control_command_result_t light_control_apply_operator_command(light_operator_request_t request,
                                                                    uint8_t cmd) {
    light_control_command_result_t result;

    result.cmd = cmd;
    result.next_request = request;
    result.accepted = false;
    result.notify = false;
    result.reason = LIGHT_CONTROL_REASON_OK;

    switch (cmd) {
        case LIGHT_CMD_LOW_BEAM_ON:
            result.next_request.low_beam_req = 1;
            result.accepted = true;
            result.notify = true;
            break;
        case LIGHT_CMD_LOW_BEAM_OFF:
            result.next_request.low_beam_req = 0;
            result.next_request.high_beam_req = 0;
            result.accepted = true;
            result.notify = true;
            break;
        case LIGHT_CMD_HIGH_BEAM_ON:
            result.next_request.high_beam_req = 1;
            result.accepted = true;
            result.notify = true;
            break;
        case LIGHT_CMD_HIGH_BEAM_OFF:
            result.next_request.high_beam_req = 0;
            result.accepted = true;
            result.notify = true;
            break;
        case LIGHT_CMD_LEFT_TURN_ON:
            result.next_request.left_turn_req = 1;
            result.next_request.right_turn_req = 0;
            result.accepted = true;
            result.notify = true;
            break;
        case LIGHT_CMD_LEFT_TURN_OFF:
            result.next_request.left_turn_req = 0;
            result.accepted = true;
            result.notify = true;
            break;
        case LIGHT_CMD_RIGHT_TURN_ON:
            result.next_request.right_turn_req = 1;
            result.next_request.left_turn_req = 0;
            result.accepted = true;
            result.notify = true;
            break;
        case LIGHT_CMD_RIGHT_TURN_OFF:
            result.next_request.right_turn_req = 0;
            result.accepted = true;
            result.notify = true;
            break;
        case LIGHT_CMD_POSITION_ON:
            result.next_request.marker_req = 1;
            result.accepted = true;
            result.notify = true;
            break;
        case LIGHT_CMD_POSITION_OFF:
            result.next_request.marker_req = 0;
            result.accepted = true;
            result.notify = true;
            break;
        case LIGHT_CMD_BRAKE_ON:
            result.next_request.brake_req = 1;
            result.accepted = true;
            result.notify = true;
            break;
        case LIGHT_CMD_BRAKE_OFF:
            result.next_request.brake_req = 0;
            result.accepted = true;
            result.notify = true;
            break;
        default:
            result.reason = LIGHT_CONTROL_REASON_INVALID_CMD;
            break;
    }

    return result;
}

light_target_output_t light_control_compute_target_output(light_operator_request_t request,
                                                          light_vehicle_state_t vehicle_state,
                                                          fault_mode_t fault_mode) {
    light_target_state_t requested_target =
        requested_target_from_request(request, vehicle_state);
    light_output_policy_result_t policy_result =
        light_output_policy_apply(requested_target, fault_mode);

    return target_output_from_state(policy_result.target);
}
