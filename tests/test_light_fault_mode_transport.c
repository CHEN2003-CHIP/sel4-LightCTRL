#include <stdio.h>
#include <stdlib.h>

#include "light_fault_mode.h"

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

static void test_initial_value_defaults_to_normal(void) {
    uint8_t slot = light_fault_mode_transport_encode(LIGHT_FAULT_MODE_NORMAL);

    expect_true(light_fault_mode_transport_load(&slot) == LIGHT_FAULT_MODE_NORMAL,
                "initial transport value should decode to NORMAL");
}

static void test_store_and_load_round_trip(void) {
    uint8_t slot = 0;

    light_fault_mode_transport_store(&slot, LIGHT_FAULT_MODE_DEGRADED);

    expect_true(slot == (uint8_t)LIGHT_FAULT_MODE_DEGRADED,
                "transport store should write DEGRADED encoding");
    expect_true(light_fault_mode_transport_load(&slot) == LIGHT_FAULT_MODE_DEGRADED,
                "transport load should read back DEGRADED");
}

static void test_continuous_mode_updates_remain_consistent(void) {
    uint8_t slot = 0;

    light_fault_mode_transport_store(&slot, LIGHT_FAULT_MODE_WARN);
    expect_true(light_fault_mode_transport_load(&slot) == LIGHT_FAULT_MODE_WARN,
                "transport should preserve WARN update");

    light_fault_mode_transport_store(&slot, LIGHT_FAULT_MODE_SAFE_MODE);
    expect_true(light_fault_mode_transport_load(&slot) == LIGHT_FAULT_MODE_SAFE_MODE,
                "transport should preserve SAFE_MODE update");

    light_fault_mode_transport_store(&slot, LIGHT_FAULT_MODE_NORMAL);
    expect_true(light_fault_mode_transport_load(&slot) == LIGHT_FAULT_MODE_NORMAL,
                "transport should preserve NORMAL update");
}

static void test_invalid_wire_value_falls_back_to_normal(void) {
    uint8_t slot = 0xff;

    expect_true(light_fault_mode_transport_decode(slot) == LIGHT_FAULT_MODE_NORMAL,
                "invalid wire value should decode to NORMAL");
    expect_true(light_fault_mode_transport_load(&slot) == LIGHT_FAULT_MODE_NORMAL,
                "transport load should clamp invalid wire value to NORMAL");
}

int main(void) {
    test_initial_value_defaults_to_normal();
    test_store_and_load_round_trip();
    test_continuous_mode_updates_remain_consistent();
    test_invalid_wire_value_falls_back_to_normal();

    printf("light_fault_mode_transport tests passed\n");
    return 0;
}
