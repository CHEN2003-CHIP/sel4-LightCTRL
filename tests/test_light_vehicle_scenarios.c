#include <stdio.h>
#include <stdlib.h>

#include "light_control_logic.h"
#include "light_vehicle_state.h"

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

static light_operator_request_t request_init(void) {
    return light_operator_request_init();
}

static light_vehicle_state_t vehicle_init(void) {
    return light_vehicle_state_default();
}

static void test_night_drive_scenario(void) {
    light_operator_request_t request = request_init();
    light_vehicle_state_t vehicle = vehicle_init();
    light_target_output_t target;

    request.marker_req = 0U;
    vehicle.ambient_light = LIGHT_VEHICLE_AMBIENT_NIGHT;
    vehicle.gear = LIGHT_VEHICLE_GEAR_DRIVE;
    vehicle.speed_kph = 45U;

    target = light_control_compute_target_output(request, vehicle, LIGHT_FAULT_MODE_NORMAL);

    expect_true(target.low_beam_on == 1U, "night drive should force low beam");
    expect_true(target.marker_on == 1U, "night drive should force marker light");
}

static void test_emergency_hazard_scenario(void) {
    light_operator_request_t request = request_init();
    light_vehicle_state_t vehicle = vehicle_init();
    light_target_output_t target;

    vehicle.drive_mode = LIGHT_VEHICLE_DRIVE_MODE_EMERGENCY;
    vehicle.speed_kph = 80U;

    target = light_control_compute_target_output(request, vehicle, LIGHT_FAULT_MODE_NORMAL);

    expect_true(target.left_turn_on == 1U, "emergency mode should drive left hazard output");
    expect_true(target.right_turn_on == 1U, "emergency mode should drive right hazard output");
}

static void test_parking_scenario_keeps_marker_and_blocks_high_beam(void) {
    light_operator_request_t request = request_init();
    light_vehicle_state_t vehicle = vehicle_init();
    light_target_output_t target;

    request.low_beam_req = 1U;
    request.high_beam_req = 1U;
    request.marker_req = 0U;
    vehicle.gear = LIGHT_VEHICLE_GEAR_PARK;
    vehicle.drive_mode = LIGHT_VEHICLE_DRIVE_MODE_PARKING;
    vehicle.speed_kph = 0U;

    target = light_control_compute_target_output(request, vehicle, LIGHT_FAULT_MODE_NORMAL);

    expect_true(target.high_beam_on == 0U, "parking scenario should block high beam");
    expect_true(target.marker_on == 1U, "parking scenario should keep marker light visible");
}

static void test_fault_overlay_clamps_vehicle_scenario(void) {
    light_operator_request_t request = request_init();
    light_vehicle_state_t vehicle = vehicle_init();
    light_target_output_t target;

    request.high_beam_req = 1U;
    vehicle.drive_mode = LIGHT_VEHICLE_DRIVE_MODE_EMERGENCY;
    vehicle.ambient_light = LIGHT_VEHICLE_AMBIENT_NIGHT;

    target = light_control_compute_target_output(request, vehicle, LIGHT_FAULT_MODE_SAFE_MODE);

    expect_true(target.high_beam_on == 0U, "safe mode should clamp high beam in a scenario");
    expect_true(target.left_turn_on == 0U, "safe mode should clamp hazard left output");
    expect_true(target.right_turn_on == 0U, "safe mode should clamp hazard right output");
    expect_true(target.low_beam_on == 1U, "safe mode should force low beam");
    expect_true(target.marker_on == 1U, "safe mode should force marker");
}

int main(void) {
    test_night_drive_scenario();
    test_emergency_hazard_scenario();
    test_parking_scenario_keeps_marker_and_blocks_high_beam();
    test_fault_overlay_clamps_vehicle_scenario();

    printf("light_vehicle_scenarios tests passed\n");
    return 0;
}
