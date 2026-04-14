#include "light_command_codec.h"

#include "light_protocol.h"

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
        case 'S':
            decoded = LIGHT_CMD_VEHICLE_SPEED_INC;
            break;
        case 's':
            decoded = LIGHT_CMD_VEHICLE_SPEED_DEC;
            break;
        case 'I':
            decoded = LIGHT_CMD_VEHICLE_IGNITION_ON;
            break;
        case 'i':
            decoded = LIGHT_CMD_VEHICLE_IGNITION_OFF;
            break;
        case 'K':
            decoded = LIGHT_CMD_VEHICLE_BRAKE_ON;
            break;
        case 'k':
            decoded = LIGHT_CMD_VEHICLE_BRAKE_OFF;
            break;
        default:
            return false;
    }

    *cmd = decoded;
    return true;
}

bool light_command_is_vehicle_state_cmd(uint8_t cmd) {
    switch (cmd) {
        case LIGHT_CMD_VEHICLE_SPEED_DEC:
        case LIGHT_CMD_VEHICLE_SPEED_INC:
        case LIGHT_CMD_VEHICLE_IGNITION_OFF:
        case LIGHT_CMD_VEHICLE_IGNITION_ON:
        case LIGHT_CMD_VEHICLE_BRAKE_OFF:
        case LIGHT_CMD_VEHICLE_BRAKE_ON:
            return true;
        default:
            return false;
    }
}
