#include <stdio.h>
#include <stdlib.h>

#include "light_vehicle_state.h"

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

static light_vehicle_state_t default_vehicle_state(void) {
    return light_vehicle_state_default();
}

static void test_speed_commands_update_and_clamp(void) {
    light_vehicle_state_t state = default_vehicle_state();
    light_vehicle_state_update_result_t result;

    result = light_vehicle_state_apply_command(state, LIGHT_CMD_VEHICLE_SPEED_INC);
    expect_true(result.accepted, "speed increment should be accepted");
    expect_true(result.next_state.speed_kph == state.speed_kph + LIGHT_VEHICLE_SPEED_STEP_KPH,
                "speed increment should add one step");

    state.speed_kph = 0U;
    result = light_vehicle_state_apply_command(state, LIGHT_CMD_VEHICLE_SPEED_DEC);
    expect_true(result.next_state.speed_kph == 0U, "speed should not go below zero");

    state.speed_kph = LIGHT_VEHICLE_SPEED_MAX_KPH;
    result = light_vehicle_state_apply_command(state, LIGHT_CMD_VEHICLE_SPEED_INC);
    expect_true(result.next_state.speed_kph == LIGHT_VEHICLE_SPEED_MAX_KPH,
                "speed should not exceed max");
}

static void test_ignition_and_brake_commands_are_explicit(void) {
    light_vehicle_state_t state = default_vehicle_state();
    light_vehicle_state_update_result_t result;

    result = light_vehicle_state_apply_command(state, LIGHT_CMD_VEHICLE_IGNITION_OFF);
    expect_true(result.accepted, "ignition off should be accepted");
    expect_true(result.next_state.ignition_on == 0U, "ignition off should clear ignition");

    result = light_vehicle_state_apply_command(result.next_state, LIGHT_CMD_VEHICLE_IGNITION_ON);
    expect_true(result.next_state.ignition_on == 1U, "ignition on should set ignition");

    result = light_vehicle_state_apply_command(result.next_state, LIGHT_CMD_VEHICLE_BRAKE_ON);
    expect_true(result.next_state.brake_pedal == 1U, "brake on should set brake pedal");

    result = light_vehicle_state_apply_command(result.next_state, LIGHT_CMD_VEHICLE_BRAKE_OFF);
    expect_true(result.next_state.brake_pedal == 0U, "brake off should clear brake pedal");
}

static void test_repeated_state_write_is_accepted_but_not_changed(void) {
    light_vehicle_state_t state = default_vehicle_state();
    light_vehicle_state_update_result_t result;

    state.ignition_on = 1U;
    result = light_vehicle_state_apply_command(state, LIGHT_CMD_VEHICLE_IGNITION_ON);

    expect_true(result.accepted, "repeated ignition on should still be accepted");
    expect_true(!result.changed, "repeated ignition on should not report state change");
}

static void test_invalid_vehicle_command_is_rejected(void) {
    light_vehicle_state_t state = default_vehicle_state();
    light_vehicle_state_update_result_t result =
        light_vehicle_state_apply_command(state, LIGHT_CMD_LOW_BEAM_ON);

    expect_true(!result.accepted, "non-vehicle command should be rejected");
    expect_true(result.reason == LIGHT_VEHICLE_STATE_REASON_INVALID_CMD,
                "non-vehicle command should report invalid reason");
}

int main(void) {
    test_speed_commands_update_and_clamp();
    test_ignition_and_brake_commands_are_explicit();
    test_repeated_state_write_is_accepted_but_not_changed();
    test_invalid_vehicle_command_is_rejected();

    printf("light_vehicle_state tests passed\n");
    return 0;
}
