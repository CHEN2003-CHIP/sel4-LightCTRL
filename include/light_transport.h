#ifndef LIGHT_TRANSPORT_H
#define LIGHT_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "light_protocol.h"

#define LIGHT_TRANSPORT_VERSION 1U

typedef enum {
    LIGHT_TRANSPORT_MSG_INVALID = 0,
    LIGHT_TRANSPORT_MSG_LIGHT_CMD = 1,
    LIGHT_TRANSPORT_MSG_VEHICLE_STATE_UPDATE = 2,
    LIGHT_TRANSPORT_MSG_FAULT_INJECT = 3,
    LIGHT_TRANSPORT_MSG_QUERY = 4,
} light_transport_msg_type_t;

typedef enum {
    LIGHT_TRANSPORT_ROUTE_NONE = 0,
    LIGHT_TRANSPORT_ROUTE_SCHEDULER = 1,
    LIGHT_TRANSPORT_ROUTE_VEHICLE_STATE = 2,
    LIGHT_TRANSPORT_ROUTE_FAULT_MGMT = 3,
    LIGHT_TRANSPORT_ROUTE_COMMANDIN = 4,
} light_transport_route_t;

typedef enum {
    LIGHT_TRANSPORT_FEED_NEED_MORE = 0,
    LIGHT_TRANSPORT_FEED_MESSAGE_READY = 1,
    LIGHT_TRANSPORT_FEED_IGNORED = 2,
    LIGHT_TRANSPORT_FEED_ERROR = 3,
} light_transport_feed_status_t;

typedef struct {
    uint8_t version;
    uint8_t type;
    uint8_t len;
    uint8_t flags;
    union {
        uint8_t light_cmd;
        light_vehicle_state_request_t vehicle_state_update;
        uint8_t fault_error_code;
        uint8_t query_id;
    } payload;
} light_transport_message_t;

typedef struct {
    char line_buf[LIGHT_COMMAND_LINE_MAX];
    uint8_t line_len;
} light_transport_parser_t;

void light_transport_parser_init(light_transport_parser_t *parser);
light_transport_feed_status_t light_transport_parser_feed_char(light_transport_parser_t *parser,
                                                               int ch,
                                                               light_transport_message_t *message);
light_transport_route_t light_transport_route_for_message(light_transport_message_t message);

#endif
