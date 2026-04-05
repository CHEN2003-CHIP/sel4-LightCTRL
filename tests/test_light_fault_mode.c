#include <stdio.h>
#include <stdlib.h>

#include "light_fault_mode.h"
#include "light_output_policy.h"
#include "light_runtime_guard.h"
#include "light_protocol.h"

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

static light_runtime_guard_context_t default_guard_context(void) {
    light_runtime_guard_context_t context;

    context.vehicle_speed = 10;
    context.last_turn_state = 0;
    context.last_beam_state = 0;
    context.last_brake_state = 0;
    context.last_position_state = 1;
    context.fault_mode = LIGHT_FAULT_MODE_NORMAL;

    return context;
}

static void test_single_minor_error_enters_warn(void) {
    light_fault_state_t state = light_fault_state_init();
    fault_decision_t decision = light_fault_mode_record_error(&state, LIGHT_ERR_SPEED_LIMIT);

    expect_true(decision.current_mode == LIGHT_FAULT_MODE_WARN,
                "single speed limit error should enter WARN");
    expect_true(decision.mode_changed, "single speed limit error should change mode");
}

static void test_repeated_mode_conflict_enters_degraded(void) {
    light_fault_state_t state = light_fault_state_init();
    fault_decision_t decision;

    decision = light_fault_mode_record_error(&state, LIGHT_ERR_MODE_CONFLICT);
    decision = light_fault_mode_record_error(&state, LIGHT_ERR_MODE_CONFLICT);
    decision = light_fault_mode_record_error(&state, LIGHT_ERR_MODE_CONFLICT);

    expect_true(decision.current_mode == LIGHT_FAULT_MODE_DEGRADED,
                "three consecutive mode conflicts should enter DEGRADED");
    expect_true(state.counters.consecutive_mode_conflicts == 3U,
                "consecutive mode conflict counter should be tracked");
}

static void test_repeated_hw_error_enters_safe_mode(void) {
    light_fault_state_t state = light_fault_state_init();
    fault_decision_t decision;

    decision = light_fault_mode_record_error(&state, LIGHT_ERR_HW_STATE_ERR);
    decision = light_fault_mode_record_error(&state, LIGHT_ERR_HW_STATE_ERR);

    expect_true(decision.current_mode == LIGHT_FAULT_MODE_SAFE_MODE,
                "two hardware state errors should enter SAFE_MODE");
}

static void test_non_conflict_error_resets_conflict_streak(void) {
    light_fault_state_t state = light_fault_state_init();
    fault_decision_t decision;

    decision = light_fault_mode_record_error(&state, LIGHT_ERR_MODE_CONFLICT);
    decision = light_fault_mode_record_error(&state, LIGHT_ERR_MODE_CONFLICT);
    decision = light_fault_mode_record_error(&state, LIGHT_ERR_SPEED_LIMIT);

    expect_true(decision.current_mode == LIGHT_FAULT_MODE_WARN,
                "non-conflict error should keep mode below DEGRADED when streak is broken");
    expect_true(state.counters.consecutive_mode_conflicts == 0U,
                "non-conflict error should reset consecutive mode conflict counter");
}

static void test_fault_event_carries_current_owner_mode(void) {
    light_fault_event_t event = light_fault_event_create(LIGHT_ERR_MODE_CONFLICT,
                                                         LIGHT_FAULT_MODE_DEGRADED);

    expect_true(event.error_code == LIGHT_ERR_MODE_CONFLICT,
                "fault event should preserve reported error code");
    expect_true(event.current_mode == LIGHT_FAULT_MODE_DEGRADED,
                "fault event should carry owner-selected mode");
}

static light_target_state_t requested_target(bool brake,
                                             bool turn_left,
                                             bool turn_right,
                                             bool low_beam,
                                             bool high_beam,
                                             bool position) {
    light_target_state_t target;

    target.brake = brake;
    target.turn_left = turn_left;
    target.turn_right = turn_right;
    target.low_beam = low_beam;
    target.high_beam = high_beam;
    target.position = position;

    return target;
}

static void test_policy_matrix_matches_normal_mode_semantics(void) {
    const light_output_policy_matrix_t *matrix =
        light_output_policy_matrix_for_mode(LIGHT_FAULT_MODE_NORMAL);

    expect_true(matrix->brake == LIGHT_OUTPUT_RULE_PASSTHROUGH, "NORMAL brake should pass through");
    expect_true(matrix->turn_left == LIGHT_OUTPUT_RULE_PASSTHROUGH, "NORMAL turn-left should pass through");
    expect_true(matrix->turn_right == LIGHT_OUTPUT_RULE_PASSTHROUGH, "NORMAL turn-right should pass through");
    expect_true(matrix->low_beam == LIGHT_OUTPUT_RULE_PASSTHROUGH, "NORMAL low beam should pass through");
    expect_true(matrix->high_beam == LIGHT_OUTPUT_RULE_PASSTHROUGH, "NORMAL high beam should pass through");
    expect_true(matrix->position == LIGHT_OUTPUT_RULE_PASSTHROUGH, "NORMAL position should pass through");
    expect_true(!matrix->force_minimum_illumination, "NORMAL should not force minimum illumination");
}

static void test_policy_matrix_matches_degraded_mode_semantics(void) {
    const light_output_policy_matrix_t *matrix =
        light_output_policy_matrix_for_mode(LIGHT_FAULT_MODE_DEGRADED);

    expect_true(matrix->brake == LIGHT_OUTPUT_RULE_PASSTHROUGH, "DEGRADED brake should pass through");
    expect_true(matrix->turn_left == LIGHT_OUTPUT_RULE_PASSTHROUGH, "DEGRADED turn-left should pass through");
    expect_true(matrix->turn_right == LIGHT_OUTPUT_RULE_PASSTHROUGH, "DEGRADED turn-right should pass through");
    expect_true(matrix->low_beam == LIGHT_OUTPUT_RULE_PASSTHROUGH, "DEGRADED low beam should pass through");
    expect_true(matrix->high_beam == LIGHT_OUTPUT_RULE_FORCE_OFF, "DEGRADED high beam should be forced off");
    expect_true(matrix->position == LIGHT_OUTPUT_RULE_PASSTHROUGH, "DEGRADED position should pass through");
    expect_true(matrix->force_minimum_illumination, "DEGRADED should enforce minimum illumination");
}

static void test_policy_matrix_matches_safe_mode_semantics(void) {
    const light_output_policy_matrix_t *matrix =
        light_output_policy_matrix_for_mode(LIGHT_FAULT_MODE_SAFE_MODE);

    expect_true(matrix->brake == LIGHT_OUTPUT_RULE_PASSTHROUGH, "SAFE_MODE brake should pass through");
    expect_true(matrix->turn_left == LIGHT_OUTPUT_RULE_PASSTHROUGH, "SAFE_MODE turn-left should pass through");
    expect_true(matrix->turn_right == LIGHT_OUTPUT_RULE_PASSTHROUGH, "SAFE_MODE turn-right should pass through");
    expect_true(matrix->low_beam == LIGHT_OUTPUT_RULE_FORCE_ON, "SAFE_MODE low beam should be forced on");
    expect_true(matrix->high_beam == LIGHT_OUTPUT_RULE_FORCE_OFF, "SAFE_MODE high beam should be forced off");
    expect_true(matrix->position == LIGHT_OUTPUT_RULE_FORCE_ON, "SAFE_MODE position should be forced on");
    expect_true(matrix->force_minimum_illumination, "SAFE_MODE should enforce minimum illumination");
}

static void test_warn_mode_keeps_requested_output(void) {
    light_output_policy_result_t result =
        light_output_policy_apply(requested_target(false, true, false, false, true, false),
                                  LIGHT_FAULT_MODE_WARN);

    expect_true(!result.changed, "WARN should preserve requested output");
    expect_true(result.target.turn_left, "WARN should keep turn output");
    expect_true(result.target.high_beam, "WARN should keep high beam output");
    expect_true(!result.target.position, "WARN should not force position output");
}

static void test_degraded_mode_downgrades_high_beam_to_low_beam(void) {
    light_output_policy_result_t result =
        light_output_policy_apply(requested_target(false, false, false, false, true, true),
                                  LIGHT_FAULT_MODE_DEGRADED);

    expect_true(result.changed, "DEGRADED should change high beam requests");
    expect_true(!result.target.high_beam, "DEGRADED should disable high beam");
    expect_true(result.target.low_beam, "DEGRADED should preserve forward lighting via low beam");
    expect_true(result.target.position, "DEGRADED should preserve existing position output");
}

static void test_degraded_mode_keeps_turn_and_brake_signals(void) {
    light_output_policy_result_t result =
        light_output_policy_apply(requested_target(true, true, false, false, false, false),
                                  LIGHT_FAULT_MODE_DEGRADED);

    expect_true(result.changed, "DEGRADED should add minimum illumination when dark");
    expect_true(result.target.brake, "DEGRADED should preserve brake output");
    expect_true(result.target.turn_left, "DEGRADED should preserve turn-left output");
    expect_true(!result.target.turn_right, "DEGRADED should not invent opposite turn output");
    expect_true(result.target.low_beam, "DEGRADED should enforce minimum forward illumination");
    expect_true(!result.target.high_beam, "DEGRADED should keep high beam off");
}

static void test_safe_mode_forces_conservative_output_profile(void) {
    light_output_policy_result_t result =
        light_output_policy_apply(requested_target(true, true, false, false, true, false),
                                  LIGHT_FAULT_MODE_SAFE_MODE);

    expect_true(result.changed, "SAFE_MODE should clamp risky output");
    expect_true(result.target.brake, "SAFE_MODE should preserve brake output");
    expect_true(result.target.turn_left, "SAFE_MODE should preserve requested turn output");
    expect_true(!result.target.turn_right, "SAFE_MODE should not invent opposite turn output");
    expect_true(!result.target.high_beam, "SAFE_MODE should disable high beam");
    expect_true(result.target.low_beam, "SAFE_MODE should force low beam");
    expect_true(result.target.position, "SAFE_MODE should force position light");
}

static void test_degraded_mode_blocks_high_beam(void) {
    light_runtime_guard_context_t context = default_guard_context();
    light_runtime_guard_result_t result;

    context.fault_mode = LIGHT_FAULT_MODE_DEGRADED;
    result = light_runtime_guard_check_action(LIGHT_CH_GPIO_HIGH_BEAM_ON, context);

    expect_true(!result.allowed, "DEGRADED should block high beam on");
    expect_true(result.report_fault, "DEGRADED high beam block should report fault");
}

static void test_safe_mode_blocks_turn_actions(void) {
    light_runtime_guard_context_t context = default_guard_context();
    light_runtime_guard_result_t result;

    context.fault_mode = LIGHT_FAULT_MODE_SAFE_MODE;
    result = light_runtime_guard_check_action(LIGHT_CH_GPIO_TURN_LEFT_ON, context);

    expect_true(!result.allowed, "SAFE_MODE should block turn actions");
    expect_true(result.error_code == LIGHT_ERR_HW_STATE_ERR,
                "SAFE_MODE turn block should use hardware-state error");
}

int main(void) {
    test_single_minor_error_enters_warn();
    test_repeated_mode_conflict_enters_degraded();
    test_repeated_hw_error_enters_safe_mode();
    test_non_conflict_error_resets_conflict_streak();
    test_fault_event_carries_current_owner_mode();
    test_policy_matrix_matches_normal_mode_semantics();
    test_policy_matrix_matches_degraded_mode_semantics();
    test_policy_matrix_matches_safe_mode_semantics();
    test_warn_mode_keeps_requested_output();
    test_degraded_mode_downgrades_high_beam_to_low_beam();
    test_degraded_mode_keeps_turn_and_brake_signals();
    test_safe_mode_forces_conservative_output_profile();
    test_degraded_mode_blocks_high_beam();
    test_safe_mode_blocks_turn_actions();

    printf("light_fault_mode tests passed\n");
    return 0;
}
