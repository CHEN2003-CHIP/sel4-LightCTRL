#include "light_vehicle_state.h"

static bool request_field_is_known(light_vehicle_state_request_t request) {
    switch ((light_vehicle_field_t)request.field) {
        case LIGHT_VEHICLE_FIELD_SPEED_KPH:
        case LIGHT_VEHICLE_FIELD_IGNITION_ON:
        case LIGHT_VEHICLE_FIELD_BRAKE_PEDAL:
        case LIGHT_VEHICLE_FIELD_GEAR:
        case LIGHT_VEHICLE_FIELD_AMBIENT_LIGHT:
        case LIGHT_VEHICLE_FIELD_HAZARD:
        case LIGHT_VEHICLE_FIELD_DRIVE_MODE:
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
        case LIGHT_VEHICLE_FIELD_HAZARD:
            return request.value <= 1U;
        case LIGHT_VEHICLE_FIELD_GEAR:
            return request.value <= LIGHT_VEHICLE_GEAR_MAX;
        case LIGHT_VEHICLE_FIELD_AMBIENT_LIGHT:
            return request.value <= LIGHT_VEHICLE_AMBIENT_MAX;
        case LIGHT_VEHICLE_FIELD_DRIVE_MODE:
            return request.value <= LIGHT_VEHICLE_DRIVE_MODE_MAX;
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
        case LIGHT_VEHICLE_FIELD_GEAR:
            result.next_state.gear = (uint8_t)request.value;
            result.accepted = true;
            break;
        case LIGHT_VEHICLE_FIELD_AMBIENT_LIGHT:
            result.next_state.ambient_light = (uint8_t)request.value;
            result.accepted = true;
            break;
        case LIGHT_VEHICLE_FIELD_HAZARD:
            result.next_state.hazard = (uint8_t)request.value;
            result.accepted = true;
            break;
        case LIGHT_VEHICLE_FIELD_DRIVE_MODE:
            result.next_state.drive_mode = (uint8_t)request.value;
            result.accepted = true;
            break;
    }

    result.changed = result.next_state.speed_kph != state.speed_kph
        || result.next_state.ignition_on != state.ignition_on
        || result.next_state.brake_pedal != state.brake_pedal
        || result.next_state.gear != state.gear
        || result.next_state.ambient_light != state.ambient_light
        || result.next_state.hazard != state.hazard
        || result.next_state.drive_mode != state.drive_mode;

    return result;
}

const char *light_vehicle_gear_name(uint8_t gear) {
    switch (gear) {
        case LIGHT_VEHICLE_GEAR_PARK:
            return "PARK";
        case LIGHT_VEHICLE_GEAR_REVERSE:
            return "REVERSE";
        case LIGHT_VEHICLE_GEAR_NEUTRAL:
            return "NEUTRAL";
        case LIGHT_VEHICLE_GEAR_DRIVE:
            return "DRIVE";
        default:
            return "UNKNOWN";
    }
}

const char *light_vehicle_ambient_light_name(uint8_t ambient_light) {
    switch (ambient_light) {
        case LIGHT_VEHICLE_AMBIENT_DAY:
            return "DAY";
        case LIGHT_VEHICLE_AMBIENT_DUSK:
            return "DUSK";
        case LIGHT_VEHICLE_AMBIENT_NIGHT:
            return "NIGHT";
        default:
            return "UNKNOWN";
    }
}

const char *light_vehicle_drive_mode_name(uint8_t drive_mode) {
    switch (drive_mode) {
        case LIGHT_VEHICLE_DRIVE_MODE_CITY:
            return "CITY";
        case LIGHT_VEHICLE_DRIVE_MODE_HIGHWAY:
            return "HIGHWAY";
        case LIGHT_VEHICLE_DRIVE_MODE_PARKING:
            return "PARKING";
        case LIGHT_VEHICLE_DRIVE_MODE_EMERGENCY:
            return "EMERGENCY";
        default:
            return "UNKNOWN";
    }
}
