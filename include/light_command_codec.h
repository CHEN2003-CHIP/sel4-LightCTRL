#ifndef LIGHT_COMMAND_CODEC_H
#define LIGHT_COMMAND_CODEC_H

#include <stdbool.h>
#include <stdint.h>

bool light_command_decode_char(int ch, uint8_t *cmd);
bool light_command_is_vehicle_state_cmd(uint8_t cmd);

#endif
