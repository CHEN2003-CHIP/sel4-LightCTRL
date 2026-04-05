#include <stdio.h>
#include <stdlib.h>

#include "light_fault_mode.h"
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
    test_degraded_mode_blocks_high_beam();
    test_safe_mode_blocks_turn_actions();

    printf("light_fault_mode tests passed\n");
    return 0;
}
