#include "light_runtime_guard.h"

#include "light_protocol.h"

static light_runtime_guard_result_t deny_with_error(uint8_t error_code) {
    light_runtime_guard_result_t result;

    result.allowed = false;
    result.report_fault = true;
    result.error_code = error_code;

    return result;
}

light_runtime_guard_result_t light_runtime_guard_check_action(uint32_t action_channel,
                                                             light_runtime_guard_context_t context) {
    light_runtime_guard_result_t result;

    result.allowed = true;
    result.report_fault = false;
    result.error_code = 0;

    if ((action_channel == LIGHT_CH_GPIO_TURN_LEFT_ON || action_channel == LIGHT_CH_GPIO_TURN_RIGHT_ON)
        && context.vehicle_speed > 120U) {
        return deny_with_error(LIGHT_ERR_SPEED_LIMIT);
    }

    if (action_channel == LIGHT_CH_GPIO_HIGH_BEAM_ON && context.vehicle_speed < 10U) {
        return deny_with_error(LIGHT_ERR_SPEED_LIMIT);
    }

    if (action_channel == LIGHT_CH_GPIO_LOW_BEAM_OFF && context.last_beam_state == 2U) {
        return deny_with_error(LIGHT_ERR_MODE_CONFLICT);
    }

    if ((action_channel == LIGHT_CH_GPIO_TURN_LEFT_ON || action_channel == LIGHT_CH_GPIO_TURN_RIGHT_ON)
        && context.last_brake_state == 1U) {
        return deny_with_error(LIGHT_ERR_MODE_CONFLICT);
    }

    return result;
}
