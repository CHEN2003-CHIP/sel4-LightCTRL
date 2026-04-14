#include "light_command_codec.h"

#include <stddef.h>

static bool parse_decimal_u16(const char *text, uint16_t *value) {
    size_t i = 0;
    uint32_t parsed = 0;

    if (text[0] == '\0') {
        return false;
    }

    while (text[i] != '\0') {
        char ch = text[i];

        if (ch < '0' || ch > '9') {
            return false;
        }
        parsed = parsed * 10U + (uint32_t)(ch - '0');
        if (parsed > 65535U) {
            return false;
        }
        i++;
    }

    *value = (uint16_t)parsed;
    return true;
}

bool light_command_decode_char(int ch, uint8_t *cmd) {
    uint8_t decoded = LIGHT_UART_CMD_INVALID;

    switch (ch) {
        case 'L':
            decoded = LIGHT_CMD_LOW_BEAM_ON;
            break;
        case 'l':
            decoded = LIGHT_CMD_LOW_BEAM_OFF;
            break;
        case 'H':
            decoded = LIGHT_CMD_HIGH_BEAM_ON;
            break;
        case 'h':
            decoded = LIGHT_CMD_HIGH_BEAM_OFF;
            break;
        case 'Z':
            decoded = LIGHT_CMD_LEFT_TURN_ON;
            break;
        case 'z':
            decoded = LIGHT_CMD_LEFT_TURN_OFF;
            break;
        case 'Y':
            decoded = LIGHT_CMD_RIGHT_TURN_ON;
            break;
        case 'y':
            decoded = LIGHT_CMD_RIGHT_TURN_OFF;
            break;
        case 'P':
            decoded = LIGHT_CMD_POSITION_ON;
            break;
        case 'p':
            decoded = LIGHT_CMD_POSITION_OFF;
            break;
        case 'B':
            decoded = LIGHT_CMD_BRAKE_ON;
            break;
        case 'b':
            decoded = LIGHT_CMD_BRAKE_OFF;
            break;
        default:
            return false;
    }

    *cmd = decoded;
    return true;
}

bool light_vehicle_state_parse_line(const char *line, light_vehicle_state_request_t *request) {
    uint16_t value = 0;

    if (line == NULL || request == NULL) {
        return false;
    }

    if (line[0] == 's' && line[1] == 'p' && line[2] == 'e' && line[3] == 'e'
        && line[4] == 'd' && line[5] == '=') {
        if (!parse_decimal_u16(&line[6], &value)) {
            return false;
        }
        request->field = LIGHT_VEHICLE_FIELD_SPEED_KPH;
        request->value = value;
        return true;
    }

    if (line[0] == 'i' && line[1] == 'g' && line[2] == 'n' && line[3] == 'i'
        && line[4] == 't' && line[5] == 'i' && line[6] == 'o' && line[7] == 'n'
        && line[8] == '=') {
        if (line[9] == '0' && line[10] == '\0') {
            request->field = LIGHT_VEHICLE_FIELD_IGNITION_ON;
            request->value = 0U;
            return true;
        }
        if (line[9] == '1' && line[10] == '\0') {
            request->field = LIGHT_VEHICLE_FIELD_IGNITION_ON;
            request->value = 1U;
            return true;
        }
        return false;
    }

    if (line[0] == 'b' && line[1] == 'r' && line[2] == 'a' && line[3] == 'k'
        && line[4] == 'e' && line[5] == '=') {
        if (line[6] == '0' && line[7] == '\0') {
            request->field = LIGHT_VEHICLE_FIELD_BRAKE_PEDAL;
            request->value = 0U;
            return true;
        }
        if (line[6] == '1' && line[7] == '\0') {
            request->field = LIGHT_VEHICLE_FIELD_BRAKE_PEDAL;
            request->value = 1U;
            return true;
        }
        return false;
    }

    return false;
}
