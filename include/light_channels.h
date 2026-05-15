#ifndef LIGHT_CHANNELS_H
#define LIGHT_CHANNELS_H

/*
 * Microkit channel endpoint IDs.
 *
 * Keep these values synchronized with light.system. The names describe the
 * endpoint as seen by the component using the ID, not a separate wire protocol.
 */

#define LIGHT_CH_COMMANDIN_UART_IRQ              0

#define LIGHT_CH_GPIO_TO_LIGHTCTL                1
#define LIGHT_CH_LIGHTCTL_FROM_GPIO              2

#define LIGHT_CH_COMMANDIN_TO_SCHEDULER          3
#define LIGHT_CH_SCHEDULER_FROM_COMMANDIN        4

#define LIGHT_CH_FAULTMG_FROM_LIGHTCTL           5
#define LIGHT_CH_LIGHTCTL_TO_FAULTMG             6

#define LIGHT_CH_FAULTMG_TO_GPIO                 7
#define LIGHT_CH_GPIO_FROM_FAULTMG               8

#define LIGHT_CH_SCHEDULER_TO_LIGHTCTL           9
#define LIGHT_CH_LIGHTCTL_FROM_SCHEDULER         10

#define LIGHT_CH_COMMANDIN_TO_FAULTMG            11
#define LIGHT_CH_FAULTMG_FROM_COMMANDIN          12

#define LIGHT_CH_SCHEDULER_FROM_FAULTMG          13
#define LIGHT_CH_FAULTMG_TO_SCHEDULER            14

#define LIGHT_CH_SCHEDULER_FROM_VEHICLE_STATE    15
#define LIGHT_CH_VEHICLE_STATE_TO_SCHEDULER      16

#define LIGHT_CH_COMMANDIN_TO_VEHICLE_STATE      17
#define LIGHT_CH_VEHICLE_STATE_FROM_COMMANDIN    18

#define LIGHT_CH_GPIO_TURN_LEFT_ON               20
#define LIGHT_CH_GPIO_TURN_LEFT_OFF              21
#define LIGHT_CH_GPIO_TURN_RIGHT_ON              22
#define LIGHT_CH_GPIO_TURN_RIGHT_OFF             23
#define LIGHT_CH_GPIO_BRAKE_ON                   24
#define LIGHT_CH_GPIO_BRAKE_OFF                  25
#define LIGHT_CH_GPIO_LOW_BEAM_ON                26
#define LIGHT_CH_GPIO_LOW_BEAM_OFF               27
#define LIGHT_CH_GPIO_HIGH_BEAM_ON               28
#define LIGHT_CH_GPIO_HIGH_BEAM_OFF              29
#define LIGHT_CH_GPIO_POSITION_ON                30
#define LIGHT_CH_GPIO_POSITION_OFF               31

#endif
