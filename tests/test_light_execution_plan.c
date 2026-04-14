#include <stdio.h>
#include <stdlib.h>

#include "light_execution_plan.h"

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

static light_target_output_t target_output(uint8_t low,
                                           uint8_t high,
                                           uint8_t left,
                                           uint8_t right,
                                           uint8_t marker,
                                           uint8_t brake) {
    light_target_output_t target;

    target.low_beam_on = low;
    target.high_beam_on = high;
    target.left_turn_on = left;
    target.right_turn_on = right;
    target.marker_on = marker;
    target.brake_on = brake;

    return target;
}

static void test_diff_generates_expected_initial_actions(void) {
    light_execution_state_t state = light_execution_state_init();
    light_execution_plan_t plan =
        light_execution_plan_build(state, target_output(1, 0, 1, 0, 1, 0));

    expect_true(plan.action_count == 3U, "initial target should generate three actions");
    expect_true(plan.actions[0] == LIGHT_CH_GPIO_TURN_LEFT_ON, "first action should enable left turn");
    expect_true(plan.actions[1] == LIGHT_CH_GPIO_LOW_BEAM_ON, "second action should enable low beam");
    expect_true(plan.actions[2] == LIGHT_CH_GPIO_POSITION_ON, "third action should enable marker");
}

static void test_repeated_sync_generates_no_actions(void) {
    light_execution_state_t state = light_execution_state_init();
    light_execution_plan_t first =
        light_execution_plan_build(state, target_output(1, 0, 0, 0, 1, 1));
    light_execution_plan_t second =
        light_execution_plan_build(first.next_state, target_output(1, 0, 0, 0, 1, 1));

    expect_true(second.action_count == 0U, "repeated sync should not emit duplicate actions");
}

static void test_beam_transition_turns_previous_output_off(void) {
    light_execution_state_t state = light_execution_state_init();
    light_execution_plan_t first =
        light_execution_plan_build(state, target_output(0, 1, 0, 0, 1, 0));
    light_execution_plan_t second =
        light_execution_plan_build(first.next_state, target_output(1, 0, 0, 0, 1, 0));

    expect_true(second.action_count == 2U, "high-to-low transition should emit two actions");
    expect_true(second.actions[0] == LIGHT_CH_GPIO_HIGH_BEAM_OFF,
                "beam transition should disable high beam before low beam");
    expect_true(second.actions[1] == LIGHT_CH_GPIO_LOW_BEAM_ON,
                "beam transition should then enable low beam");
}

static void test_turn_transition_turns_previous_side_off(void) {
    light_execution_state_t state = light_execution_state_init();
    light_execution_plan_t first =
        light_execution_plan_build(state, target_output(0, 0, 1, 0, 1, 0));
    light_execution_plan_t second =
        light_execution_plan_build(first.next_state, target_output(0, 0, 0, 1, 1, 0));

    expect_true(second.action_count == 2U, "left-to-right transition should emit two actions");
    expect_true(second.actions[0] == LIGHT_CH_GPIO_TURN_LEFT_OFF,
                "turn transition should disable previous side first");
    expect_true(second.actions[1] == LIGHT_CH_GPIO_TURN_RIGHT_ON,
                "turn transition should enable requested side second");
}

static void test_safe_mode_target_executes_as_clamped_target(void) {
    light_execution_state_t state = light_execution_state_init();
    light_execution_plan_t plan =
        light_execution_plan_build(state, target_output(1, 0, 0, 0, 1, 0));

    expect_true(plan.action_count == 2U, "safe-mode style target should only emit low beam and marker");
    expect_true(plan.actions[0] == LIGHT_CH_GPIO_LOW_BEAM_ON, "plan should enable low beam");
    expect_true(plan.actions[1] == LIGHT_CH_GPIO_POSITION_ON, "plan should enable marker");
}

int main(void) {
    test_diff_generates_expected_initial_actions();
    test_repeated_sync_generates_no_actions();
    test_beam_transition_turns_previous_output_off();
    test_turn_transition_turns_previous_side_off();
    test_safe_mode_target_executes_as_clamped_target();

    printf("light_execution_plan tests passed\n");
    return 0;
}
