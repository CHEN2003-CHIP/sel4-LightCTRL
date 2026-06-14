#include "light_command_codec.h"

#include <stddef.h>

#include "light_vehicle_state.h"

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

static bool literal_equals(const char *text, const char *expected) {
    size_t i = 0;

    while (text[i] != '\0' && expected[i] != '\0') {
        if (text[i] != expected[i]) {
            return false;
        }
        i++;
    }

    return text[i] == '\0' && expected[i] == '\0';
}

static bool parse_named_or_numeric(const char *text,
                                   const char *name0,
                                   const char *name1,
                                   const char *name2,
                                   const char *name3,
                                   uint16_t max_value,
                                   uint16_t *value) {
    if (literal_equals(text, name0)) {
        *value = 0U;
        return true;
    }
    if (literal_equals(text, name1)) {
        *value = 1U;
        return true;
    }
    if (literal_equals(text, name2)) {
        *value = 2U;
        return true;
    }
    if (name3 != NULL && literal_equals(text, name3)) {
        *value = 3U;
        return true;
    }
    if (!parse_decimal_u16(text, value)) {
        return false;
    }
    return *value <= max_value;
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

    if (line[0] == 'g' && line[1] == 'e' && line[2] == 'a' && line[3] == 'r'
        && line[4] == '=') {
        if (!parse_named_or_numeric(&line[5], "park", "reverse", "neutral", "drive",
                                    LIGHT_VEHICLE_GEAR_MAX, &value)) {
            return false;
        }
        request->field = LIGHT_VEHICLE_FIELD_GEAR;
        request->value = value;
        return true;
    }

    if (line[0] == 'a' && line[1] == 'm' && line[2] == 'b' && line[3] == 'i'
        && line[4] == 'e' && line[5] == 'n' && line[6] == 't' && line[7] == '=') {
        if (!parse_named_or_numeric(&line[8], "day", "dusk", "night", NULL,
                                    LIGHT_VEHICLE_AMBIENT_MAX, &value)) {
            return false;
        }
        request->field = LIGHT_VEHICLE_FIELD_AMBIENT_LIGHT;
        request->value = value;
        return true;
    }

    if (line[0] == 'h' && line[1] == 'a' && line[2] == 'z' && line[3] == 'a'
        && line[4] == 'r' && line[5] == 'd' && line[6] == '=') {
        if (line[7] == '0' && line[8] == '\0') {
            request->field = LIGHT_VEHICLE_FIELD_HAZARD;
            request->value = 0U;
            return true;
        }
        if (line[7] == '1' && line[8] == '\0') {
            request->field = LIGHT_VEHICLE_FIELD_HAZARD;
            request->value = 1U;
            return true;
        }
        return false;
    }

    if (line[0] == 'm' && line[1] == 'o' && line[2] == 'd' && line[3] == 'e'
        && line[4] == '=') {
        if (!parse_named_or_numeric(&line[5], "city", "highway", "parking", "emergency",
                                    LIGHT_VEHICLE_DRIVE_MODE_MAX, &value)) {
            return false;
        }
        request->field = LIGHT_VEHICLE_FIELD_DRIVE_MODE;
        request->value = value;
        return true;
    }

    return false;
}
