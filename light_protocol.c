#include "light_protocol.h"

light_operator_request_t light_operator_request_init(void) {
    light_operator_request_t request;

    request.low_beam_req = 0;
    request.high_beam_req = 0;
    request.left_turn_req = 0;
    request.right_turn_req = 0;
    request.marker_req = 1;
    request.brake_req = 0;

    return request;
}

light_vehicle_state_t light_vehicle_state_default(void) {
    light_vehicle_state_t vehicle_state;

    vehicle_state.speed_kph = 10;
    vehicle_state.brake_pedal = 0;
    vehicle_state.ignition_on = 1;

    return vehicle_state;
}

light_target_output_t light_target_output_init(void) {
    light_target_output_t target_output;

    target_output.low_beam_on = 0;
    target_output.high_beam_on = 0;
    target_output.left_turn_on = 0;
    target_output.right_turn_on = 0;
    target_output.marker_on = 1;
    target_output.brake_on = 0;

    return target_output;
}

uint32_t light_target_output_to_allow_flags(light_target_output_t target_output) {
    uint32_t allow_flags = 0;

    if (target_output.brake_on != 0U) {
        allow_flags |= LIGHT_ALLOW_BRAKE;
    }
    if (target_output.left_turn_on != 0U) {
        allow_flags |= LIGHT_ALLOW_TURN_LEFT;
    }
    if (target_output.right_turn_on != 0U) {
        allow_flags |= LIGHT_ALLOW_TURN_RIGHT;
    }
    if (target_output.low_beam_on != 0U) {
        allow_flags |= LIGHT_ALLOW_LOW_BEAM;
    }
    if (target_output.high_beam_on != 0U) {
        allow_flags |= LIGHT_ALLOW_HIGH_BEAM;
    }
    if (target_output.marker_on != 0U) {
        allow_flags |= LIGHT_ALLOW_POSITION;
    }

    return allow_flags;
}

light_target_output_t light_target_output_from_allow_flags(uint32_t allow_flags) {
    light_target_output_t target_output;

    target_output.brake_on = ((allow_flags & LIGHT_ALLOW_BRAKE) != 0U) ? 1U : 0U;
    target_output.left_turn_on = ((allow_flags & LIGHT_ALLOW_TURN_LEFT) != 0U) ? 1U : 0U;
    target_output.right_turn_on = ((allow_flags & LIGHT_ALLOW_TURN_RIGHT) != 0U) ? 1U : 0U;
    target_output.low_beam_on = ((allow_flags & LIGHT_ALLOW_LOW_BEAM) != 0U) ? 1U : 0U;
    target_output.high_beam_on = ((allow_flags & LIGHT_ALLOW_HIGH_BEAM) != 0U) ? 1U : 0U;
    target_output.marker_on = ((allow_flags & LIGHT_ALLOW_POSITION) != 0U) ? 1U : 0U;

    return target_output;
}
