#include "light_transport.h"

#include "light_command_codec.h"

static void parser_reset(light_transport_parser_t *parser) {
    parser->line_len = 0;
    parser->line_buf[0] = '\0';
}

static light_transport_message_t make_light_cmd_message(uint8_t cmd) {
    light_transport_message_t message;

    message.version = LIGHT_TRANSPORT_VERSION;
    message.type = LIGHT_TRANSPORT_MSG_LIGHT_CMD;
    message.len = sizeof(message.payload.light_cmd);
    message.flags = 0;
    message.payload.light_cmd = cmd;

    return message;
}

static light_transport_message_t make_vehicle_state_message(light_vehicle_state_request_t request) {
    light_transport_message_t message;

    message.version = LIGHT_TRANSPORT_VERSION;
    message.type = LIGHT_TRANSPORT_MSG_VEHICLE_STATE_UPDATE;
    message.len = sizeof(message.payload.vehicle_state_update);
    message.flags = 0;
    message.payload.vehicle_state_update = request;

    return message;
}

static light_transport_message_t make_fault_inject_message(uint8_t error_code) {
    light_transport_message_t message;

    message.version = LIGHT_TRANSPORT_VERSION;
    message.type = LIGHT_TRANSPORT_MSG_FAULT_INJECT;
    message.len = sizeof(message.payload.fault_error_code);
    message.flags = 0;
    message.payload.fault_error_code = error_code;

    return message;
}

static light_transport_message_t make_query_message(void) {
    light_transport_message_t message;

    message.version = LIGHT_TRANSPORT_VERSION;
    message.type = LIGHT_TRANSPORT_MSG_QUERY;
    message.len = sizeof(message.payload.query_id);
    message.flags = 0;
    message.payload.query_id = 0U;

    return message;
}

static light_transport_message_t make_fault_clear_message(uint8_t scope) {
    light_transport_message_t message;

    message.version = LIGHT_TRANSPORT_VERSION;
    message.type = LIGHT_TRANSPORT_MSG_FAULT_CLEAR;
    message.len = sizeof(message.payload.fault_clear_scope);
    message.flags = 0;
    message.payload.fault_clear_scope = scope;

    return message;
}

void light_transport_parser_init(light_transport_parser_t *parser) {
    parser_reset(parser);
}

light_transport_feed_status_t light_transport_parser_feed_char(light_transport_parser_t *parser,
                                                               int ch,
                                                               light_transport_message_t *message) {
    uint8_t light_cmd = LIGHT_UART_CMD_INVALID;
    light_vehicle_state_request_t request;

    if (ch == '\r') {
        if (parser->line_len == 0U) {
            return LIGHT_TRANSPORT_FEED_IGNORED;
        }
        parser->line_buf[parser->line_len] = '\0';
        if (!light_vehicle_state_parse_line(parser->line_buf, &request)) {
            parser_reset(parser);
            return LIGHT_TRANSPORT_FEED_ERROR;
        }
        *message = make_vehicle_state_message(request);
        parser_reset(parser);
        return LIGHT_TRANSPORT_FEED_MESSAGE_READY;
    }

    if (parser->line_len == 0U) {
        if (ch == '!') {
            *message = make_fault_inject_message(LIGHT_ERR_MODE_CONFLICT);
            return LIGHT_TRANSPORT_FEED_MESSAGE_READY;
        }
        if (ch == '#') {
            *message = make_fault_inject_message(LIGHT_ERR_HW_STATE_ERR);
            return LIGHT_TRANSPORT_FEED_MESSAGE_READY;
        }
        if (ch == '?') {
            *message = make_query_message();
            return LIGHT_TRANSPORT_FEED_MESSAGE_READY;
        }
        if (ch == 'C') {
            *message = make_fault_clear_message(LIGHT_TRANSPORT_FAULT_CLEAR_ALL);
            return LIGHT_TRANSPORT_FEED_MESSAGE_READY;
        }
    }

    if (parser->line_len == 0U && light_command_decode_char(ch, &light_cmd)) {
        *message = make_light_cmd_message(light_cmd);
        return LIGHT_TRANSPORT_FEED_MESSAGE_READY;
    }

    if (parser->line_len + 1U >= LIGHT_COMMAND_LINE_MAX) {
        parser_reset(parser);
        return LIGHT_TRANSPORT_FEED_ERROR;
    }

    parser->line_buf[parser->line_len++] = (char)ch;
    parser->line_buf[parser->line_len] = '\0';
    return LIGHT_TRANSPORT_FEED_NEED_MORE;
}

light_transport_route_t light_transport_route_for_message(light_transport_message_t message) {
    switch ((light_transport_msg_type_t)message.type) {
        case LIGHT_TRANSPORT_MSG_LIGHT_CMD:
            return LIGHT_TRANSPORT_ROUTE_SCHEDULER;
        case LIGHT_TRANSPORT_MSG_VEHICLE_STATE_UPDATE:
            return LIGHT_TRANSPORT_ROUTE_VEHICLE_STATE;
        case LIGHT_TRANSPORT_MSG_FAULT_INJECT:
            return LIGHT_TRANSPORT_ROUTE_FAULT_MGMT;
        case LIGHT_TRANSPORT_MSG_QUERY:
            return LIGHT_TRANSPORT_ROUTE_COMMANDIN;
        case LIGHT_TRANSPORT_MSG_FAULT_CLEAR:
            return LIGHT_TRANSPORT_ROUTE_FAULT_MGMT;
        case LIGHT_TRANSPORT_MSG_INVALID:
        default:
            return LIGHT_TRANSPORT_ROUTE_NONE;
    }
}
