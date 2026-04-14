#include <stdio.h>
#include <stdlib.h>

#include "light_command_codec.h"
#include "light_protocol.h"

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

static void test_existing_light_commands_still_decode(void) {
    uint8_t cmd = LIGHT_UART_CMD_INVALID;

    expect_true(light_command_decode_char('L', &cmd), "L should decode");
    expect_true(cmd == LIGHT_CMD_LOW_BEAM_ON, "L should map to low beam on");

    expect_true(light_command_decode_char('H', &cmd), "H should decode");
    expect_true(cmd == LIGHT_CMD_HIGH_BEAM_ON, "H should map to high beam on");

    expect_true(light_command_decode_char('Z', &cmd), "Z should decode");
    expect_true(cmd == LIGHT_CMD_LEFT_TURN_ON, "Z should map to left turn on");
}

static void test_vehicle_state_protocol_parses_structured_requests(void) {
    light_vehicle_state_request_t request;

    expect_true(light_vehicle_state_parse_line("speed=120", &request),
                "speed assignment should parse");
    expect_true(request.field == LIGHT_VEHICLE_FIELD_SPEED_KPH,
                "speed assignment should target speed field");
    expect_true(request.value == 120U, "speed assignment should preserve explicit value");

    expect_true(light_vehicle_state_parse_line("ignition=1", &request),
                "ignition assignment should parse");
    expect_true(request.field == LIGHT_VEHICLE_FIELD_IGNITION_ON,
                "ignition assignment should target ignition field");
    expect_true(request.value == 1U, "ignition assignment should preserve explicit value");

    expect_true(light_vehicle_state_parse_line("brake=0", &request),
                "brake assignment should parse");
    expect_true(request.field == LIGHT_VEHICLE_FIELD_BRAKE_PEDAL,
                "brake assignment should target brake field");
    expect_true(request.value == 0U, "brake assignment should preserve explicit value");
}

static void test_invalid_command_is_rejected(void) {
    uint8_t cmd = LIGHT_UART_CMD_INVALID;

    expect_true(!light_command_decode_char('?', &cmd), "unknown command should be rejected");
    expect_true(!light_vehicle_state_parse_line("speed=-1", NULL),
                "null request target should be rejected safely");
}

static void test_invalid_vehicle_state_lines_are_rejected(void) {
    light_vehicle_state_request_t request;

    expect_true(!light_vehicle_state_parse_line("speed=-1", &request),
                "negative speed should be rejected");
    expect_true(!light_vehicle_state_parse_line("ignition=2", &request),
                "invalid ignition value should be rejected");
    expect_true(!light_vehicle_state_parse_line("brake=2", &request),
                "invalid brake value should be rejected");
    expect_true(!light_vehicle_state_parse_line("unknown=1", &request),
                "unknown field should be rejected");
}

int main(void) {
    test_existing_light_commands_still_decode();
    test_vehicle_state_protocol_parses_structured_requests();
    test_invalid_command_is_rejected();
    test_invalid_vehicle_state_lines_are_rejected();

    printf("light_command_codec tests passed\n");
    return 0;
}
