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

static void test_vehicle_state_commands_decode_to_vehicle_channel_commands(void) {
    uint8_t cmd = LIGHT_UART_CMD_INVALID;

    expect_true(light_command_decode_char('S', &cmd), "S should decode");
    expect_true(cmd == LIGHT_CMD_VEHICLE_SPEED_INC, "S should map to vehicle speed increment");
    expect_true(light_command_is_vehicle_state_cmd(cmd), "speed increment should target vehicle_state");

    expect_true(light_command_decode_char('i', &cmd), "i should decode");
    expect_true(cmd == LIGHT_CMD_VEHICLE_IGNITION_OFF, "i should map to ignition off");
    expect_true(light_command_is_vehicle_state_cmd(cmd), "ignition off should target vehicle_state");

    expect_true(light_command_decode_char('K', &cmd), "K should decode");
    expect_true(cmd == LIGHT_CMD_VEHICLE_BRAKE_ON, "K should map to brake pedal on");
    expect_true(light_command_is_vehicle_state_cmd(cmd), "brake pedal on should target vehicle_state");
}

static void test_invalid_command_is_rejected(void) {
    uint8_t cmd = LIGHT_UART_CMD_INVALID;

    expect_true(!light_command_decode_char('?', &cmd), "unknown command should be rejected");
    expect_true(!light_command_is_vehicle_state_cmd(LIGHT_CMD_LOW_BEAM_ON),
                "light command should not be classified as vehicle-state command");
}

int main(void) {
    test_existing_light_commands_still_decode();
    test_vehicle_state_commands_decode_to_vehicle_channel_commands();
    test_invalid_command_is_rejected();

    printf("light_command_codec tests passed\n");
    return 0;
}
