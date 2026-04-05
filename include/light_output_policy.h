#ifndef LIGHT_OUTPUT_POLICY_H
#define LIGHT_OUTPUT_POLICY_H

#include <stdbool.h>

#include "light_fault_mode.h"
#include "light_policy.h"

typedef enum {
    LIGHT_OUTPUT_RULE_PASSTHROUGH = 0,
    LIGHT_OUTPUT_RULE_FORCE_ON = 1,
    LIGHT_OUTPUT_RULE_FORCE_OFF = 2,
} light_output_rule_t;

typedef struct {
    light_output_rule_t brake;
    light_output_rule_t turn_left;
    light_output_rule_t turn_right;
    light_output_rule_t low_beam;
    light_output_rule_t high_beam;
    light_output_rule_t position;
    bool force_minimum_illumination;
} light_output_policy_matrix_t;

typedef struct {
    light_target_state_t target;
    bool changed;
} light_output_policy_result_t;

const light_output_policy_matrix_t *light_output_policy_matrix_for_mode(fault_mode_t fault_mode);
light_output_policy_result_t light_output_policy_apply(light_target_state_t requested_target,
                                                       fault_mode_t fault_mode);

#endif
