#include <stdio.h>
#include <stdlib.h>

#include "light_protocol.h"

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

static void test_default_shared_state_v2_values(void) {
    light_operator_request_t request = light_operator_request_init();
    light_vehicle_state_t vehicle_state = light_vehicle_state_default();
    light_target_output_t target_output = light_target_output_init();

    expect_true(request.marker_req == 1U, "default operator request should keep marker enabled");
    expect_true(request.high_beam_req == 0U, "default operator request should keep high beam off");
    expect_true(vehicle_state.speed_kph == 10U, "default vehicle speed should be 10kph");
    expect_true(vehicle_state.ignition_on == 1U, "default ignition should be on");
    expect_true(target_output.marker_on == 1U, "default target output should keep marker on");
}

static void test_target_output_allow_flags_round_trip(void) {
    light_target_output_t target_output;
    uint32_t allow_flags;
    light_target_output_t round_trip;

    target_output.low_beam_on = 1;
    target_output.high_beam_on = 0;
    target_output.left_turn_on = 1;
    target_output.right_turn_on = 0;
    target_output.marker_on = 1;
    target_output.brake_on = 1;

    allow_flags = light_target_output_to_allow_flags(target_output);
    round_trip = light_target_output_from_allow_flags(allow_flags);

    expect_true(round_trip.low_beam_on == 1U, "round trip should preserve low beam");
    expect_true(round_trip.left_turn_on == 1U, "round trip should preserve left turn");
    expect_true(round_trip.marker_on == 1U, "round trip should preserve marker");
    expect_true(round_trip.brake_on == 1U, "round trip should preserve brake");
    expect_true(round_trip.high_beam_on == 0U, "round trip should preserve high beam off");
}

int main(void) {
    test_default_shared_state_v2_values();
    test_target_output_allow_flags_round_trip();

    printf("light_protocol tests passed\n");
    return 0;
}
