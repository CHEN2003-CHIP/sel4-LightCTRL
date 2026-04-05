#include <stdio.h>
#include <stdlib.h>

#include "light_runtime_guard.h"
#include "light_protocol.h"

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

static light_runtime_guard_context_t default_context(void) {
    light_runtime_guard_context_t context;

    context.vehicle_speed = 10;
    context.last_turn_state = 0;
    context.last_beam_state = 0;
    context.last_brake_state = 0;
    context.last_position_state = 1;

    return context;
}

static void test_high_beam_rejected_when_speed_too_low(void) {
    light_runtime_guard_context_t context = default_context();
    light_runtime_guard_result_t result;

    context.vehicle_speed = 5;
    result = light_runtime_guard_check_action(LIGHT_CH_GPIO_HIGH_BEAM_ON, context);

    expect_true(!result.allowed, "high beam should be rejected at low speed");
    expect_true(result.report_fault, "high beam rejection should report fault");
    expect_true(result.error_code == LIGHT_ERR_SPEED_LIMIT,
                "high beam low-speed rejection should use speed limit error");
}

static void test_turn_rejected_when_brake_history_active(void) {
    light_runtime_guard_context_t context = default_context();
    light_runtime_guard_result_t result;

    context.last_brake_state = 1;
    result = light_runtime_guard_check_action(LIGHT_CH_GPIO_TURN_LEFT_ON, context);

    expect_true(!result.allowed, "turn should be rejected while brake history active");
    expect_true(result.report_fault, "turn rejection should report fault");
    expect_true(result.error_code == LIGHT_ERR_MODE_CONFLICT,
                "turn rejection should use mode conflict error");
}

static void test_historical_beam_conflict_reports_error(void) {
    light_runtime_guard_context_t context = default_context();
    light_runtime_guard_result_t result;

    context.last_beam_state = 2;
    result = light_runtime_guard_check_action(LIGHT_CH_GPIO_LOW_BEAM_OFF, context);

    expect_true(!result.allowed, "low beam off should be rejected while high beam history active");
    expect_true(result.report_fault, "beam conflict should report fault");
    expect_true(result.error_code == LIGHT_ERR_MODE_CONFLICT,
                "beam conflict should use mode conflict error");
}

static void test_legal_turn_does_not_report_fault(void) {
    light_runtime_guard_context_t context = default_context();
    light_runtime_guard_result_t result =
        light_runtime_guard_check_action(LIGHT_CH_GPIO_TURN_RIGHT_ON, context);

    expect_true(result.allowed, "legal turn should be allowed");
    expect_true(!result.report_fault, "legal turn should not report fault");
    expect_true(result.error_code == 0, "legal turn should not set an error code");
}

int main(void) {
    test_high_beam_rejected_when_speed_too_low();
    test_turn_rejected_when_brake_history_active();
    test_historical_beam_conflict_reports_error();
    test_legal_turn_does_not_report_fault();

    printf("light_runtime_guard tests passed\n");
    return 0;
}
