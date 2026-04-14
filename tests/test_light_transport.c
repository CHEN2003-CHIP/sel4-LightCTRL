#include <stdio.h>
#include <stdlib.h>

#include "light_transport.h"

static void expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

static void test_single_char_light_command_becomes_complete_message(void) {
    light_transport_parser_t parser;
    light_transport_message_t message;
    light_transport_feed_status_t status;

    light_transport_parser_init(&parser);
    status = light_transport_parser_feed_char(&parser, 'L', &message);

    expect_true(status == LIGHT_TRANSPORT_FEED_MESSAGE_READY,
                "single-char light command should produce a complete message");
    expect_true(message.type == LIGHT_TRANSPORT_MSG_LIGHT_CMD,
                "light command should map to light message type");
    expect_true(message.payload.light_cmd == LIGHT_CMD_LOW_BEAM_ON,
                "light command should preserve opcode");
    expect_true(light_transport_route_for_message(message) == LIGHT_TRANSPORT_ROUTE_SCHEDULER,
                "light command should route to scheduler");
}

static void test_vehicle_state_line_becomes_complete_message_on_commit(void) {
    light_transport_parser_t parser;
    light_transport_message_t message;
    light_transport_feed_status_t status;

    light_transport_parser_init(&parser);
    status = light_transport_parser_feed_char(&parser, 's', &message);
    expect_true(status == LIGHT_TRANSPORT_FEED_NEED_MORE,
                "partial vehicle-state input should wait for more data");
    status = light_transport_parser_feed_char(&parser, 'p', &message);
    expect_true(status == LIGHT_TRANSPORT_FEED_NEED_MORE,
                "vehicle-state parser should keep buffering");
    status = light_transport_parser_feed_char(&parser, 'e', &message);
    status = light_transport_parser_feed_char(&parser, 'e', &message);
    status = light_transport_parser_feed_char(&parser, 'd', &message);
    status = light_transport_parser_feed_char(&parser, '=', &message);
    status = light_transport_parser_feed_char(&parser, '4', &message);
    status = light_transport_parser_feed_char(&parser, '0', &message);
    status = light_transport_parser_feed_char(&parser, '\r', &message);

    expect_true(status == LIGHT_TRANSPORT_FEED_MESSAGE_READY,
                "vehicle-state line should produce a complete message on commit");
    expect_true(message.type == LIGHT_TRANSPORT_MSG_VEHICLE_STATE_UPDATE,
                "vehicle-state line should map to update message type");
    expect_true(message.payload.vehicle_state_update.field == LIGHT_VEHICLE_FIELD_SPEED_KPH,
                "vehicle-state message should preserve field");
    expect_true(message.payload.vehicle_state_update.value == 40U,
                "vehicle-state message should preserve explicit value");
    expect_true(light_transport_route_for_message(message) == LIGHT_TRANSPORT_ROUTE_VEHICLE_STATE,
                "vehicle-state update should route to vehicle_state owner");
}

static void test_unfinished_input_is_not_submitted(void) {
    light_transport_parser_t parser;
    light_transport_message_t message;
    light_transport_feed_status_t status;

    light_transport_parser_init(&parser);
    status = light_transport_parser_feed_char(&parser, 'i', &message);

    expect_true(status == LIGHT_TRANSPORT_FEED_NEED_MORE,
                "unfinished vehicle-state input should not submit");
}

static void test_fault_inject_char_becomes_fault_message(void) {
    light_transport_parser_t parser;
    light_transport_message_t message;
    light_transport_feed_status_t status;

    light_transport_parser_init(&parser);
    status = light_transport_parser_feed_char(&parser, '!', &message);

    expect_true(status == LIGHT_TRANSPORT_FEED_MESSAGE_READY,
                "fault inject char should produce a complete message");
    expect_true(message.type == LIGHT_TRANSPORT_MSG_FAULT_INJECT,
                "fault inject char should map to fault message type");
    expect_true(message.payload.fault_error_code == LIGHT_ERR_MODE_CONFLICT,
                "fault inject char should preserve error code");
    expect_true(light_transport_route_for_message(message) == LIGHT_TRANSPORT_ROUTE_FAULT_MGMT,
                "fault inject message should route to fault management");
}

static void test_query_char_becomes_query_message(void) {
    light_transport_parser_t parser;
    light_transport_message_t message;
    light_transport_feed_status_t status;

    light_transport_parser_init(&parser);
    status = light_transport_parser_feed_char(&parser, '?', &message);

    expect_true(status == LIGHT_TRANSPORT_FEED_MESSAGE_READY,
                "query char should produce a complete message");
    expect_true(message.type == LIGHT_TRANSPORT_MSG_QUERY,
                "query char should map to query message type");
    expect_true(light_transport_route_for_message(message) == LIGHT_TRANSPORT_ROUTE_COMMANDIN,
                "query message should route to commandin snapshot path");
}

static void test_fault_clear_char_becomes_fault_clear_message(void) {
    light_transport_parser_t parser;
    light_transport_message_t message;
    light_transport_feed_status_t status;

    light_transport_parser_init(&parser);
    status = light_transport_parser_feed_char(&parser, 'C', &message);

    expect_true(status == LIGHT_TRANSPORT_FEED_MESSAGE_READY,
                "fault clear char should produce a complete message");
    expect_true(message.type == LIGHT_TRANSPORT_MSG_FAULT_CLEAR,
                "fault clear char should map to fault clear message type");
    expect_true(message.payload.fault_clear_scope == LIGHT_TRANSPORT_FAULT_CLEAR_ALL,
                "fault clear char should default to clear-all scope");
    expect_true(light_transport_route_for_message(message) == LIGHT_TRANSPORT_ROUTE_FAULT_MGMT,
                "fault clear message should route to fault management");
}

static void test_invalid_input_is_rejected_safely(void) {
    light_transport_parser_t parser;
    light_transport_message_t message;
    light_transport_feed_status_t status;

    light_transport_parser_init(&parser);
    status = light_transport_parser_feed_char(&parser, 'u', &message);
    expect_true(status == LIGHT_TRANSPORT_FEED_NEED_MORE,
                "unknown leading text should be buffered as a potential line");
    status = light_transport_parser_feed_char(&parser, 'n', &message);
    status = light_transport_parser_feed_char(&parser, 'k', &message);
    status = light_transport_parser_feed_char(&parser, '\r', &message);

    expect_true(status == LIGHT_TRANSPORT_FEED_ERROR,
                "invalid structured line should be rejected on commit");
}

int main(void) {
    test_single_char_light_command_becomes_complete_message();
    test_vehicle_state_line_becomes_complete_message_on_commit();
    test_unfinished_input_is_not_submitted();
    test_fault_inject_char_becomes_fault_message();
    test_query_char_becomes_query_message();
    test_fault_clear_char_becomes_fault_clear_message();
    test_invalid_input_is_rejected_safely();

    printf("light_transport tests passed\n");
    return 0;
}
