#include "light_vehicle_state.h"

static bool request_field_is_known(light_vehicle_state_request_t request) {
    switch ((light_vehicle_field_t)request.field) {
        case LIGHT_VEHICLE_FIELD_SPEED_KPH:
        case LIGHT_VEHICLE_FIELD_IGNITION_ON:
        case LIGHT_VEHICLE_FIELD_BRAKE_PEDAL:
            return true;
        default:
            return false;
    }
}

static bool request_value_is_valid(light_vehicle_state_request_t request) {
    switch ((light_vehicle_field_t)request.field) {
        case LIGHT_VEHICLE_FIELD_SPEED_KPH:
            return request.value <= LIGHT_VEHICLE_SPEED_MAX_KPH;
        case LIGHT_VEHICLE_FIELD_IGNITION_ON:
        case LIGHT_VEHICLE_FIELD_BRAKE_PEDAL:
            return request.value <= 1U;
        default:
            return false;
    }
}

light_vehicle_state_update_result_t light_vehicle_state_apply_request(light_vehicle_state_t state,
                                                                      light_vehicle_state_request_t request) {
    light_vehicle_state_update_result_t result;

    result.request = request;
    result.next_state = state;
    result.accepted = false;
    result.changed = false;
    result.reason = LIGHT_VEHICLE_STATE_REASON_OK;

    if (!request_field_is_known(request)) {
        result.reason = LIGHT_VEHICLE_STATE_REASON_INVALID_REQUEST;
        return result;
    }

    if (!request_value_is_valid(request)) {
        result.reason = LIGHT_VEHICLE_STATE_REASON_INVALID_VALUE;
        return result;
    }

    switch ((light_vehicle_field_t)request.field) {
        case LIGHT_VEHICLE_FIELD_SPEED_KPH:
            result.next_state.speed_kph = request.value;
            result.accepted = true;
            break;
        case LIGHT_VEHICLE_FIELD_IGNITION_ON:
            result.next_state.ignition_on = (uint8_t)request.value;
            result.accepted = true;
            break;
        case LIGHT_VEHICLE_FIELD_BRAKE_PEDAL:
            result.next_state.brake_pedal = (uint8_t)request.value;
            result.accepted = true;
            break;
    }

    result.changed = result.next_state.speed_kph != state.speed_kph
        || result.next_state.ignition_on != state.ignition_on
        || result.next_state.brake_pedal != state.brake_pedal;

    return result;
}
