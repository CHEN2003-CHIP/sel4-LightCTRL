#include "light_vehicle_state.h"

static uint16_t clamp_speed_inc(uint16_t speed_kph) {
    if (speed_kph >= LIGHT_VEHICLE_SPEED_MAX_KPH) {
        return LIGHT_VEHICLE_SPEED_MAX_KPH;
    }

    if ((uint32_t)speed_kph + LIGHT_VEHICLE_SPEED_STEP_KPH > LIGHT_VEHICLE_SPEED_MAX_KPH) {
        return LIGHT_VEHICLE_SPEED_MAX_KPH;
    }

    return (uint16_t)(speed_kph + LIGHT_VEHICLE_SPEED_STEP_KPH);
}

static uint16_t clamp_speed_dec(uint16_t speed_kph) {
    if (speed_kph <= LIGHT_VEHICLE_SPEED_STEP_KPH) {
        return 0U;
    }

    return (uint16_t)(speed_kph - LIGHT_VEHICLE_SPEED_STEP_KPH);
}

light_vehicle_state_update_result_t light_vehicle_state_apply_command(light_vehicle_state_t state,
                                                                      uint8_t cmd) {
    light_vehicle_state_update_result_t result;

    result.cmd = cmd;
    result.next_state = state;
    result.accepted = false;
    result.changed = false;
    result.reason = LIGHT_VEHICLE_STATE_REASON_OK;

    switch (cmd) {
        case LIGHT_CMD_VEHICLE_SPEED_DEC:
            result.next_state.speed_kph = clamp_speed_dec(state.speed_kph);
            result.accepted = true;
            break;
        case LIGHT_CMD_VEHICLE_SPEED_INC:
            result.next_state.speed_kph = clamp_speed_inc(state.speed_kph);
            result.accepted = true;
            break;
        case LIGHT_CMD_VEHICLE_IGNITION_OFF:
            result.next_state.ignition_on = 0U;
            result.accepted = true;
            break;
        case LIGHT_CMD_VEHICLE_IGNITION_ON:
            result.next_state.ignition_on = 1U;
            result.accepted = true;
            break;
        case LIGHT_CMD_VEHICLE_BRAKE_OFF:
            result.next_state.brake_pedal = 0U;
            result.accepted = true;
            break;
        case LIGHT_CMD_VEHICLE_BRAKE_ON:
            result.next_state.brake_pedal = 1U;
            result.accepted = true;
            break;
        default:
            result.reason = LIGHT_VEHICLE_STATE_REASON_INVALID_CMD;
            return result;
    }

    result.changed = result.next_state.speed_kph != state.speed_kph
        || result.next_state.ignition_on != state.ignition_on
        || result.next_state.brake_pedal != state.brake_pedal;

    return result;
}
