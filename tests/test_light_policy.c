#include <stdio.h>
#include <stdlib.h>

#include "light_policy.h"

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

static void test_default_position_enabled(void) {
    light_policy_state_t state = light_policy_init_state();
    light_target_state_t target = light_policy_target_from_flags(state.allow_flags);

    expect_true((state.allow_flags & LIGHT_ALLOW_POSITION) != 0U,
                "default position flag should be enabled");
    expect_true(target.position, "default position target should be on");
}

static void test_low_beam_on(void) {
    light_policy_state_t state = light_policy_init_state();
    light_policy_result_t result = light_policy_apply_command(state, LIGHT_CMD_LOW_BEAM_ON);
    light_target_state_t target = light_policy_target_from_flags(result.next_allow_flags);

    expect_true(result.accepted, "low beam on should be accepted");
    expect_true(result.notify, "low beam on should notify");
    expect_true(target.low_beam, "low beam target should be on");
    expect_true(!target.high_beam, "high beam target should remain off");
}

static void test_high_beam_requires_low_beam(void) {
    light_policy_state_t state = light_policy_init_state();
    light_policy_result_t result = light_policy_apply_command(state, LIGHT_CMD_HIGH_BEAM_ON);
    light_target_state_t target = light_policy_target_from_flags(result.next_allow_flags);

    expect_true(!result.accepted, "high beam without low beam should be rejected");
    expect_true(result.reason == LIGHT_POLICY_REJECT_LOW_BEAM_REQUIRED,
                "high beam reject reason should require low beam");
    expect_true(!target.high_beam, "high beam target should remain off");
}

static void test_turn_signals_are_mutually_exclusive(void) {
    light_policy_state_t state = light_policy_init_state();
    light_policy_result_t left_on = light_policy_apply_command(state, LIGHT_CMD_LEFT_TURN_ON);
    light_policy_result_t right_on;
    light_target_state_t target;

    expect_true(left_on.accepted, "left turn should be accepted");

    state.allow_flags = left_on.next_allow_flags;
    right_on = light_policy_apply_command(state, LIGHT_CMD_RIGHT_TURN_ON);
    target = light_policy_target_from_flags(right_on.next_allow_flags);

    expect_true(!right_on.accepted, "right turn should be rejected while left turn active");
    expect_true(right_on.reason == LIGHT_POLICY_REJECT_LEFT_TURN_ACTIVE,
                "right turn reject reason should report left turn active");
    expect_true(target.turn_left, "left turn target should remain on");
    expect_true(!target.turn_right, "right turn target should remain off");
}

static void test_brake_priority_blocks_turn_requests(void) {
    light_policy_state_t state = light_policy_init_state();
    light_policy_result_t brake_on = light_policy_apply_command(state, LIGHT_CMD_BRAKE_ON);
    light_policy_result_t left_on;
    light_target_state_t target;

    expect_true(brake_on.accepted, "brake on should be accepted");
    state.allow_flags = brake_on.next_allow_flags;

    left_on = light_policy_apply_command(state, LIGHT_CMD_LEFT_TURN_ON);
    target = light_policy_target_from_flags(left_on.next_allow_flags);

    expect_true(!left_on.accepted, "left turn should be rejected while brake active");
    expect_true(left_on.reason == LIGHT_POLICY_REJECT_BRAKE_ACTIVE,
                "left turn reject reason should report brake active");
    expect_true(target.brake, "brake target should remain on");
    expect_true(!target.turn_left, "left turn target should remain off");
}

static void test_high_beam_flag_overrides_low_beam_target(void) {
    light_policy_state_t state = light_policy_init_state();
    light_policy_result_t low_on = light_policy_apply_command(state, LIGHT_CMD_LOW_BEAM_ON);
    light_policy_result_t high_on;
    light_target_state_t target;

    state.allow_flags = low_on.next_allow_flags;
    high_on = light_policy_apply_command(state, LIGHT_CMD_HIGH_BEAM_ON);
    target = light_policy_target_from_flags(high_on.next_allow_flags);

    expect_true(high_on.accepted, "high beam should be accepted after low beam");
    expect_true(!target.low_beam, "low beam target should yield once high beam is requested");
    expect_true(target.high_beam, "high beam target should become active");
}

int main(void) {
    test_default_position_enabled();
    test_low_beam_on();
    test_high_beam_requires_low_beam();
    test_turn_signals_are_mutually_exclusive();
    test_brake_priority_blocks_turn_requests();
    test_high_beam_flag_overrides_low_beam_target();

    printf("light_policy tests passed\n");
    return 0;
}
