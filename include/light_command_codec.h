#ifndef LIGHT_COMMAND_CODEC_H
#define LIGHT_COMMAND_CODEC_H

#include <stdbool.h>
#include <stdint.h>

#include "light_protocol.h"

bool light_command_decode_char(int ch, uint8_t *cmd);
bool light_vehicle_state_parse_line(const char *line, light_vehicle_state_request_t *request);

#endif
