#include "light_output_policy.h"

static bool apply_rule(bool value, light_output_rule_t rule) {
    switch (rule) {
        case LIGHT_OUTPUT_RULE_FORCE_ON:
            return true;
        case LIGHT_OUTPUT_RULE_FORCE_OFF:
            return false;
        case LIGHT_OUTPUT_RULE_PASSTHROUGH:
        default:
            return value;
    }
}

const light_output_policy_matrix_t *light_output_policy_matrix_for_mode(fault_mode_t fault_mode) {
    static const light_output_policy_matrix_t normal_matrix = {
        .brake = LIGHT_OUTPUT_RULE_PASSTHROUGH,
        .turn_left = LIGHT_OUTPUT_RULE_PASSTHROUGH,
        .turn_right = LIGHT_OUTPUT_RULE_PASSTHROUGH,
        .low_beam = LIGHT_OUTPUT_RULE_PASSTHROUGH,
        .high_beam = LIGHT_OUTPUT_RULE_PASSTHROUGH,
        .position = LIGHT_OUTPUT_RULE_PASSTHROUGH,
        .force_minimum_illumination = false,
    };
    static const light_output_policy_matrix_t degraded_matrix = {
        .brake = LIGHT_OUTPUT_RULE_PASSTHROUGH,
        .turn_left = LIGHT_OUTPUT_RULE_PASSTHROUGH,
        .turn_right = LIGHT_OUTPUT_RULE_PASSTHROUGH,
        .low_beam = LIGHT_OUTPUT_RULE_PASSTHROUGH,
        .high_beam = LIGHT_OUTPUT_RULE_FORCE_OFF,
        .position = LIGHT_OUTPUT_RULE_PASSTHROUGH,
        .force_minimum_illumination = true,
    };
    static const light_output_policy_matrix_t safe_matrix = {
        .brake = LIGHT_OUTPUT_RULE_PASSTHROUGH,
        .turn_left = LIGHT_OUTPUT_RULE_FORCE_OFF,
        .turn_right = LIGHT_OUTPUT_RULE_FORCE_OFF,
        .low_beam = LIGHT_OUTPUT_RULE_FORCE_ON,
        .high_beam = LIGHT_OUTPUT_RULE_FORCE_OFF,
        .position = LIGHT_OUTPUT_RULE_FORCE_ON,
        .force_minimum_illumination = true,
    };

    switch (fault_mode) {
        case LIGHT_FAULT_MODE_DEGRADED:
            return &degraded_matrix;
        case LIGHT_FAULT_MODE_SAFE_MODE:
            return &safe_matrix;
        case LIGHT_FAULT_MODE_NORMAL:
        case LIGHT_FAULT_MODE_WARN:
        default:
            return &normal_matrix;
    }
}

light_output_policy_result_t light_output_policy_apply(light_target_state_t requested_target,
                                                       fault_mode_t fault_mode) {
    light_output_policy_result_t result;
    const light_output_policy_matrix_t *matrix = light_output_policy_matrix_for_mode(fault_mode);

    result.target = requested_target;
    result.changed = false;

    result.target.brake = apply_rule(result.target.brake, matrix->brake);
    result.target.turn_left = apply_rule(result.target.turn_left, matrix->turn_left);
    result.target.turn_right = apply_rule(result.target.turn_right, matrix->turn_right);
    result.target.low_beam = apply_rule(result.target.low_beam, matrix->low_beam);
    result.target.high_beam = apply_rule(result.target.high_beam, matrix->high_beam);
    result.target.position = apply_rule(result.target.position, matrix->position);

    if (matrix->force_minimum_illumination
        && !result.target.high_beam
        && !result.target.low_beam) {
        result.target.low_beam = true;
    }

    result.changed = result.target.brake != requested_target.brake
        || result.target.turn_left != requested_target.turn_left
        || result.target.turn_right != requested_target.turn_right
        || result.target.low_beam != requested_target.low_beam
        || result.target.high_beam != requested_target.high_beam
        || result.target.position != requested_target.position;

    return result;
}
