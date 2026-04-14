#include <stdio.h>
#include <stdlib.h>

#include "light_control_logic.h"

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

static light_operator_request_t default_request(void) {
    return light_operator_request_init();
}

static light_vehicle_state_t default_vehicle_state(void) {
    return light_vehicle_state_default();
}

static void test_normal_mode_passes_basic_requests(void) {
    light_operator_request_t request = default_request();
    light_vehicle_state_t vehicle_state = default_vehicle_state();
    light_target_output_t target_output;

    request.low_beam_req = 1;
    request.left_turn_req = 1;

    target_output = light_control_compute_target_output(request,
                                                        vehicle_state,
                                                        LIGHT_FAULT_MODE_NORMAL);

    expect_true(target_output.low_beam_on == 1U, "NORMAL should preserve low beam request");
    expect_true(target_output.left_turn_on == 1U, "NORMAL should preserve left turn request");
    expect_true(target_output.high_beam_on == 0U, "NORMAL should keep high beam off unless requested");
    expect_true(target_output.marker_on == 1U, "NORMAL should keep marker request");
}

static void test_degraded_mode_clamps_high_beam(void) {
    light_operator_request_t request = default_request();
    light_vehicle_state_t vehicle_state = default_vehicle_state();
    light_target_output_t target_output;

    request.low_beam_req = 1;
    request.high_beam_req = 1;

    target_output = light_control_compute_target_output(request,
                                                        vehicle_state,
                                                        LIGHT_FAULT_MODE_DEGRADED);

    expect_true(target_output.high_beam_on == 0U, "DEGRADED should block high beam");
    expect_true(target_output.low_beam_on == 1U, "DEGRADED should keep minimum forward lighting");
    expect_true(target_output.marker_on == 1U, "DEGRADED should preserve marker");
}

static void test_safe_mode_forces_conservative_target_output(void) {
    light_operator_request_t request = default_request();
    light_vehicle_state_t vehicle_state = default_vehicle_state();
    light_target_output_t target_output;

    request.high_beam_req = 1;
    request.left_turn_req = 1;
    request.marker_req = 0;

    target_output = light_control_compute_target_output(request,
                                                        vehicle_state,
                                                        LIGHT_FAULT_MODE_SAFE_MODE);

    expect_true(target_output.high_beam_on == 0U, "SAFE_MODE should disable high beam");
    expect_true(target_output.left_turn_on == 0U, "SAFE_MODE should disable turn output");
    expect_true(target_output.low_beam_on == 1U, "SAFE_MODE should force low beam");
    expect_true(target_output.marker_on == 1U, "SAFE_MODE should force marker");
}

static void test_speed_and_brake_state_limit_risky_outputs(void) {
    light_operator_request_t request = default_request();
    light_vehicle_state_t vehicle_state = default_vehicle_state();
    light_target_output_t target_output;

    request.low_beam_req = 1;
    request.high_beam_req = 1;
    request.right_turn_req = 1;
    vehicle_state.speed_kph = 130;
    vehicle_state.brake_pedal = 1;

    target_output = light_control_compute_target_output(request,
                                                        vehicle_state,
                                                        LIGHT_FAULT_MODE_NORMAL);

    expect_true(target_output.high_beam_on == 0U, "brake or speed constraints should block high beam");
    expect_true(target_output.right_turn_on == 0U, "brake or speed constraints should block turn output");
    expect_true(target_output.brake_on == 1U, "brake pedal should drive brake output");
    expect_true(target_output.low_beam_on == 1U, "low beam request should remain active");
}

static void test_ignition_off_cuts_drive_lighting_but_keeps_marker_request(void) {
    light_operator_request_t request = default_request();
    light_vehicle_state_t vehicle_state = default_vehicle_state();
    light_target_output_t target_output;

    request.low_beam_req = 1;
    request.left_turn_req = 1;
    vehicle_state.ignition_on = 0;

    target_output = light_control_compute_target_output(request,
                                                        vehicle_state,
                                                        LIGHT_FAULT_MODE_NORMAL);

    expect_true(target_output.low_beam_on == 0U, "ignition off should cut low beam");
    expect_true(target_output.left_turn_on == 0U, "ignition off should cut turn output");
    expect_true(target_output.marker_on == 1U, "ignition off should preserve marker request");
}

static void test_operator_commands_update_request_state(void) {
    light_operator_request_t request = default_request();
    light_control_command_result_t result;

    result = light_control_apply_operator_command(request, LIGHT_CMD_LOW_BEAM_ON);
    expect_true(result.accepted, "low beam command should be accepted");
    expect_true(result.next_request.low_beam_req == 1U, "low beam command should update request");

    result = light_control_apply_operator_command(result.next_request, LIGHT_CMD_HIGH_BEAM_ON);
    expect_true(result.next_request.high_beam_req == 1U, "high beam command should update request");

    result = light_control_apply_operator_command(result.next_request, LIGHT_CMD_RIGHT_TURN_ON);
    expect_true(result.next_request.right_turn_req == 1U, "right turn command should update request");
    expect_true(result.next_request.left_turn_req == 0U, "right turn command should clear opposite request");
}

int main(void) {
    test_normal_mode_passes_basic_requests();
    test_degraded_mode_clamps_high_beam();
    test_safe_mode_forces_conservative_target_output();
    test_speed_and_brake_state_limit_risky_outputs();
    test_ignition_off_cuts_drive_lighting_but_keeps_marker_request();
    test_operator_commands_update_request_state();

    printf("light_control_logic tests passed\n");
    return 0;
}
