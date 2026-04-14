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

static light_vehicle_state_request_t request(uint8_t field, uint16_t value) {
    light_vehicle_state_request_t update;

    update.field = field;
    update.value = value;

    return update;
}

static void test_speed_requests_update_with_explicit_value(void) {
    light_vehicle_state_t state = default_vehicle_state();
    light_vehicle_state_update_result_t result;

    result = light_vehicle_state_apply_request(state,
                                               request(LIGHT_VEHICLE_FIELD_SPEED_KPH, 120U));
    expect_true(result.accepted, "explicit speed set should be accepted");
    expect_true(result.next_state.speed_kph == 120U,
                "explicit speed set should preserve target value");
}

static void test_ignition_and_brake_requests_are_explicit(void) {
    light_vehicle_state_t state = default_vehicle_state();
    light_vehicle_state_update_result_t result;

    result = light_vehicle_state_apply_request(state,
                                               request(LIGHT_VEHICLE_FIELD_IGNITION_ON, 0U));
    expect_true(result.accepted, "ignition off should be accepted");
    expect_true(result.next_state.ignition_on == 0U, "ignition off should clear ignition");

    result = light_vehicle_state_apply_request(result.next_state,
                                               request(LIGHT_VEHICLE_FIELD_IGNITION_ON, 1U));
    expect_true(result.next_state.ignition_on == 1U, "ignition on should set ignition");

    result = light_vehicle_state_apply_request(result.next_state,
                                               request(LIGHT_VEHICLE_FIELD_BRAKE_PEDAL, 1U));
    expect_true(result.next_state.brake_pedal == 1U, "brake on should set brake pedal");

    result = light_vehicle_state_apply_request(result.next_state,
                                               request(LIGHT_VEHICLE_FIELD_BRAKE_PEDAL, 0U));
    expect_true(result.next_state.brake_pedal == 0U, "brake off should clear brake pedal");
}

static void test_repeated_state_write_is_idempotent(void) {
    light_vehicle_state_t state = default_vehicle_state();
    light_vehicle_state_update_result_t result;

    state.ignition_on = 1U;
    result = light_vehicle_state_apply_request(state,
                                               request(LIGHT_VEHICLE_FIELD_IGNITION_ON, 1U));

    expect_true(result.accepted, "repeated ignition on should still be accepted");
    expect_true(!result.changed, "repeated ignition on should not report state change");
}

static void test_invalid_speed_request_is_rejected_without_mutation(void) {
    light_vehicle_state_t state = default_vehicle_state();
    light_vehicle_state_update_result_t result =
        light_vehicle_state_apply_request(state,
                                          request(LIGHT_VEHICLE_FIELD_SPEED_KPH,
                                                  LIGHT_VEHICLE_SPEED_MAX_KPH + 1U));

    expect_true(!result.accepted, "speed above max should be rejected");
    expect_true(result.reason == LIGHT_VEHICLE_STATE_REASON_INVALID_VALUE,
                "speed above max should report invalid value");
    expect_true(result.next_state.speed_kph == state.speed_kph,
                "invalid speed should not mutate state");
}

static void test_invalid_field_request_is_rejected(void) {
    light_vehicle_state_t state = default_vehicle_state();
    light_vehicle_state_update_result_t result =
        light_vehicle_state_apply_request(state, request(0xffU, 1U));

    expect_true(!result.accepted, "unknown field should be rejected");
    expect_true(result.reason == LIGHT_VEHICLE_STATE_REASON_INVALID_REQUEST,
                "unknown field should report invalid request");
}

int main(void) {
    test_speed_requests_update_with_explicit_value();
    test_ignition_and_brake_requests_are_explicit();
    test_repeated_state_write_is_idempotent();
    test_invalid_speed_request_is_rejected_without_mutation();
    test_invalid_field_request_is_rejected();

    printf("light_vehicle_state tests passed\n");
    return 0;
}
