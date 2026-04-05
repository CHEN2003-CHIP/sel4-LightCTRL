#ifndef LIGHT_PROTOCOL_H
#define LIGHT_PROTOCOL_H

#include <stdint.h>

#define LIGHT_CMD_LOW_BEAM_OFF    0x00
#define LIGHT_CMD_LOW_BEAM_ON     0x01
#define LIGHT_CMD_HIGH_BEAM_OFF   0x10
#define LIGHT_CMD_HIGH_BEAM_ON    0x11
#define LIGHT_CMD_LEFT_TURN_OFF   0x20
#define LIGHT_CMD_LEFT_TURN_ON    0x21
#define LIGHT_CMD_RIGHT_TURN_OFF  0x30
#define LIGHT_CMD_RIGHT_TURN_ON   0x31
#define LIGHT_CMD_POSITION_OFF    0x40
#define LIGHT_CMD_POSITION_ON     0x41
#define LIGHT_CMD_BRAKE_OFF       0x50
#define LIGHT_CMD_BRAKE_ON        0x51

#define LIGHT_ERR_SPEED_LIMIT     0x01
#define LIGHT_ERR_MODE_CONFLICT   0x02
#define LIGHT_ERR_INVALID_CMD     0x03
#define LIGHT_ERR_HW_STATE_ERR    0x04

#define LIGHT_ALLOW_BRAKE         (1UL << 0)
#define LIGHT_ALLOW_TURN_LEFT     (1UL << 1)
#define LIGHT_ALLOW_TURN_RIGHT    (1UL << 2)
#define LIGHT_ALLOW_LOW_BEAM      (1UL << 3)
#define LIGHT_ALLOW_HIGH_BEAM     (1UL << 4)
#define LIGHT_ALLOW_POSITION      (1UL << 5)

#define LIGHT_FLAG_IS_SET(flags, mask) (((flags) & (mask)) != 0)

#define LIGHT_UART_CMD_INVALID    0xFF

typedef struct {
    volatile uint8_t uart_cmd;
    volatile uint32_t allow_flags;
    volatile uint8_t turn_switch_pos;
    volatile uint8_t beam_switch_pos;
    volatile uint16_t vehicle_speed;
} light_shmem_t;

#define LIGHT_CH_GPIO_TURN_LEFT_ON    20
#define LIGHT_CH_GPIO_TURN_LEFT_OFF   21
#define LIGHT_CH_GPIO_TURN_RIGHT_ON   22
#define LIGHT_CH_GPIO_TURN_RIGHT_OFF  23
#define LIGHT_CH_GPIO_BRAKE_ON        24
#define LIGHT_CH_GPIO_BRAKE_OFF       25
#define LIGHT_CH_GPIO_LOW_BEAM_ON     26
#define LIGHT_CH_GPIO_LOW_BEAM_OFF    27
#define LIGHT_CH_GPIO_HIGH_BEAM_ON    28
#define LIGHT_CH_GPIO_HIGH_BEAM_OFF   29
#define LIGHT_CH_GPIO_POSITION_ON     30
#define LIGHT_CH_GPIO_POSITION_OFF    31

#endif
