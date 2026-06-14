/**
 * @file commandin.c
 * @brief UART input gateway for the lighting control system.
 * @details Receives PL011 UART input, parses it into light transport messages,
 *          and dispatches each message to scheduler, vehicle_state, faultmg,
 *          or the local status snapshot path.
 * @author USTC-CHEN
 * @date 2025-12-05
 * @note This component owns command parsing only. Fault lifecycle ownership
 *       stays in faultmg.
 */


#include <stdint.h>
#include <stdbool.h>
#include <microkit.h>
#include "printf.h"
#include <stdatomic.h>
#include <stdio.h>
#include "logger.h"
#include "light_contract.h"
#include "light_fault_mode.h"
#include "light_protocol.h"
#include "light_transport.h"
#include "light_vehicle_state.h"


/**
 * @var uart_base_vaddr
 * @brief UART设备硬件寄存器基地址
 * @note 由Microkit系统描述文件配置，指向UART外设的虚拟地址
 */
uintptr_t uart_base_vaddr;
uintptr_t shared_memory_base_vaddr;

uintptr_t input_buffer;  // 由系统描述文件的setvar_vaddr自动赋值
uintptr_t shared_memory_base_vaddr;

#define SHARED_BUF_SIZE 0x1000  


/* UART寄存器操作掩码及偏移量定义 */
#define RHR_MASK         0b111111111       /* UART接收数据寄存器掩码 */
#define UARTDR           0x000             /* UART数据寄存器偏移 */
#define UARTFR           0x018             /* UART标志寄存器偏移 */
#define UARTIMSC         0x038             /* UART中断屏蔽寄存器偏移 */
#define UARTICR          0x044             /* UART中断清除寄存器偏移 */
#define PL011_UARTFR_TXFF (1 << 5)        /* UART发送FIFO满标志位 */
#define PL011_UARTFR_RXFE (1 << 4)        /* UART接收FIFO空标志位 */

#define DEMO_ANSI_RESET   "\x1b[0m"
#define DEMO_ANSI_BOLD    "\x1b[1m"
#define DEMO_ANSI_DIM     "\x1b[2m"
#define DEMO_ANSI_GREEN   "\x1b[1;32m"
#define DEMO_ANSI_YELLOW  "\x1b[1;33m"
#define DEMO_ANSI_BLUE    "\x1b[1;34m"
#define DEMO_ANSI_MAGENTA "\x1b[1;35m"
#define DEMO_ANSI_CYAN    "\x1b[1;36m"
#define DEMO_ANSI_RED     "\x1b[1;31m"

#if LIGHT_ENABLE_TEST_HOOKS
#define TEST_FAULT_MODE_CONFLICT '!'
#define TEST_FAULT_HW_STATE '#'
#endif

static light_transport_parser_t g_transport_parser;
static light_shmem_t *g_shmem = NULL;
static const char *shared_state_contract_name(void);

/**
 * @def REG_PTR(base, offset)
 * @brief 计算寄存器虚拟地址
 * @param base 寄存器基地址
 * @param offset 寄存器偏移量
 * @return 寄存器的volatile uint32_t类型指针
 */
#define REG_PTR(base, offset) ((volatile uint32_t *)((base) + (offset)))

/**
 * @brief UART设备初始化函数
 * @details 配置UART中断屏蔽寄存器，初始化串口通信环境
 * @param 无
 * @return 无
 */
void uart_init() {
    *REG_PTR(uart_base_vaddr, UARTIMSC) = 0x50;
}

/**
 * @brief 从UART获取一个字符
 * @details 检查接收FIFO状态，读取有效字符并做格式转换（换行转回车、退格转DEL）
 * @param 无
 * @return int 读取到的字符ASCII码，无数据时返回0
 */
int uart_get_char() {
    
    int ch = 0;

    if ((*REG_PTR(uart_base_vaddr, UARTFR) & PL011_UARTFR_RXFE) == 0) {
        ch = *REG_PTR(uart_base_vaddr, UARTDR) & RHR_MASK;
    }

    /*
     * Convert Newline to Carriage return; backspace to DEL
     */
    switch (ch) {
    case '\n':
        ch = '\r';
        break;
    case 8:
        ch = 0x7f;
        break;
    }
    return ch;
}

/**
 * @brief 向UART发送一个字符
 * @details 等待发送FIFO非满后写入字符，回车符自动追加换行符
 * @param ch 待发送的字符ASCII码
 * @return 无
 */
void uart_put_char(int ch) {
    while ((*REG_PTR(uart_base_vaddr, UARTFR) & PL011_UARTFR_TXFF) != 0);

    *REG_PTR(uart_base_vaddr, UARTDR) = ch;
    if (ch == '\r') {
        uart_put_char('\n');
    }
}

/**
 * @brief UART中断处理函数
 * @details 清除UART所有中断标志位，完成中断响应
 * @param 无
 * @return 无
 */
void uart_handle_irq() {
    *REG_PTR(uart_base_vaddr, UARTICR) = 0x7f0;
}

/**
 * @brief 向UART发送字符串
 * @details 逐字符调用uart_put_char发送，直到字符串结束符
 * @param str 待发送的字符串指针
 * @return 无
 */
void uart_put_str(const char *str) {
    while (*str) {
        uart_put_char(*str);
        str++;
    }
}

static void uart_put_u32(uint32_t value) {
    char digits[10];
    size_t count = 0;

    if (value == 0U) {
        uart_put_char('0');
        return;
    }

    while (value > 0U) {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (count > 0U) {
        uart_put_char(digits[--count]);
    }
}

static void uart_put_hex8(uint8_t value) {
    static const char hex_digits[] = "0123456789abcdef";

    uart_put_str("0x");
    uart_put_char(hex_digits[(value >> 4) & 0x0fU]);
    uart_put_char(hex_digits[value & 0x0fU]);
}

static const char *demo_light_cmd_name(uint8_t cmd) {
    switch (cmd) {
        case LIGHT_CMD_LOW_BEAM_ON:
            return "LOW_BEAM_ON";
        case LIGHT_CMD_LOW_BEAM_OFF:
            return "LOW_BEAM_OFF";
        case LIGHT_CMD_HIGH_BEAM_ON:
            return "HIGH_BEAM_ON";
        case LIGHT_CMD_HIGH_BEAM_OFF:
            return "HIGH_BEAM_OFF";
        case LIGHT_CMD_LEFT_TURN_ON:
            return "LEFT_TURN_ON";
        case LIGHT_CMD_LEFT_TURN_OFF:
            return "LEFT_TURN_OFF";
        case LIGHT_CMD_RIGHT_TURN_ON:
            return "RIGHT_TURN_ON";
        case LIGHT_CMD_RIGHT_TURN_OFF:
            return "RIGHT_TURN_OFF";
        case LIGHT_CMD_POSITION_ON:
            return "POSITION_ON";
        case LIGHT_CMD_POSITION_OFF:
            return "POSITION_OFF";
        case LIGHT_CMD_BRAKE_ON:
            return "BRAKE_ON";
        case LIGHT_CMD_BRAKE_OFF:
            return "BRAKE_OFF";
        default:
            return "UNKNOWN";
    }
}

static const char *demo_vehicle_field_name(uint8_t field) {
    switch ((light_vehicle_field_t)field) {
        case LIGHT_VEHICLE_FIELD_SPEED_KPH:
            return "speed";
        case LIGHT_VEHICLE_FIELD_IGNITION_ON:
            return "ignition";
        case LIGHT_VEHICLE_FIELD_BRAKE_PEDAL:
            return "brake";
        case LIGHT_VEHICLE_FIELD_GEAR:
            return "gear";
        case LIGHT_VEHICLE_FIELD_AMBIENT_LIGHT:
            return "ambient";
        case LIGHT_VEHICLE_FIELD_HAZARD:
            return "hazard";
        case LIGHT_VEHICLE_FIELD_DRIVE_MODE:
            return "mode";
        default:
            return "unknown";
    }
}

static const char *demo_route_name(light_transport_route_t route) {
    switch (route) {
        case LIGHT_TRANSPORT_ROUTE_SCHEDULER:
            return "commandin->scheduler";
        case LIGHT_TRANSPORT_ROUTE_VEHICLE_STATE:
            return "commandin->vehicle_state->scheduler";
        case LIGHT_TRANSPORT_ROUTE_FAULT_MGMT:
            return "commandin->faultmg->scheduler/gpio";
        case LIGHT_TRANSPORT_ROUTE_COMMANDIN:
            return "commandin->status_snapshot";
        case LIGHT_TRANSPORT_ROUTE_NONE:
        default:
            return "none";
    }
}

static const char *demo_fault_color(void) {
    switch ((fault_mode_t)g_shmem->fault_mode) {
        case LIGHT_FAULT_MODE_NORMAL:
            return DEMO_ANSI_GREEN;
        case LIGHT_FAULT_MODE_WARN:
            return DEMO_ANSI_YELLOW;
        case LIGHT_FAULT_MODE_DEGRADED:
            return DEMO_ANSI_MAGENTA;
        case LIGHT_FAULT_MODE_SAFE_MODE:
            return DEMO_ANSI_RED;
        default:
            return DEMO_ANSI_RED;
    }
}

static void uart_put_lamp_state(const char *name, uint8_t enabled) {
    uart_put_str(name);
    uart_put_char('=');
    if (enabled != 0U) {
        uart_put_str(DEMO_ANSI_GREEN);
        uart_put_str("ON ");
        uart_put_str(DEMO_ANSI_RESET);
    } else {
        uart_put_str(DEMO_ANSI_DIM);
        uart_put_str("off ");
        uart_put_str(DEMO_ANSI_RESET);
    }
}

static void emit_demo_input_line(light_transport_message_t message, light_transport_route_t route) {
    uart_put_str("\r");
    uart_put_str(DEMO_ANSI_CYAN);
    uart_put_str("[INPUT] ");
    uart_put_str(DEMO_ANSI_RESET);

    if (message.type == LIGHT_TRANSPORT_MSG_LIGHT_CMD) {
        uart_put_str("light command ");
        uart_put_str(DEMO_ANSI_BOLD);
        uart_put_str(demo_light_cmd_name(message.payload.light_cmd));
        uart_put_str(DEMO_ANSI_RESET);
    } else if (message.type == LIGHT_TRANSPORT_MSG_VEHICLE_STATE_UPDATE) {
        uart_put_str("vehicle ");
        uart_put_str(DEMO_ANSI_BOLD);
        uart_put_str(demo_vehicle_field_name(message.payload.vehicle_state_update.field));
        uart_put_char('=');
        uart_put_u32(message.payload.vehicle_state_update.value);
        uart_put_str(DEMO_ANSI_RESET);
    } else if (message.type == LIGHT_TRANSPORT_MSG_FAULT_INJECT) {
        uart_put_str("fault inject ");
        uart_put_str(DEMO_ANSI_RED);
        uart_put_str(light_fault_code_name(message.payload.fault_error_code));
        uart_put_str(DEMO_ANSI_RESET);
    } else if (message.type == LIGHT_TRANSPORT_MSG_FAULT_CLEAR) {
        uart_put_str("fault clear / recovery tick");
    } else if (message.type == LIGHT_TRANSPORT_MSG_QUERY) {
        uart_put_str("status query");
    } else {
        uart_put_str("unknown input");
    }

    uart_put_str("  route=");
    uart_put_str(DEMO_ANSI_BLUE);
    uart_put_str(demo_route_name(route));
    uart_put_str(DEMO_ANSI_RESET);
    uart_put_str("\r");
}

static void emit_demo_status_panel(void) {
    uart_put_str("\r");
    uart_put_str(DEMO_ANSI_BOLD);
    uart_put_str("+--------------------------------------------------------------------------+\r");
    uart_put_str("| LightDemo Engineering Final v2.0 - Live Status                           |\r");
    uart_put_str("+--------------------------------------------------------------------------+\r");
    uart_put_str(DEMO_ANSI_RESET);
    uart_put_str("| fault=");
    uart_put_str(demo_fault_color());
    uart_put_str(light_fault_mode_name((fault_mode_t)g_shmem->fault_mode));
    uart_put_str(DEMO_ANSI_RESET);
    uart_put_str(" lifecycle=");
    uart_put_str(light_fault_lifecycle_name((light_fault_lifecycle_t)g_shmem->fault_lifecycle));
    uart_put_str(" recovery=");
    uart_put_u32(g_shmem->fault_recovery_elapsed_ms);
    uart_put_char('/');
    uart_put_u32(light_fault_recovery_window_ms());
    uart_put_str("ms\r");
    uart_put_str("| vehicle speed=");
    uart_put_u32(g_shmem->vehicle_state.speed_kph);
    uart_put_str(" ignition=");
    uart_put_u32(g_shmem->vehicle_state.ignition_on);
    uart_put_str(" brake=");
    uart_put_u32(g_shmem->vehicle_state.brake_pedal);
    uart_put_str(" gear=");
    uart_put_str(light_vehicle_gear_name(g_shmem->vehicle_state.gear));
    uart_put_str(" ambient=");
    uart_put_str(light_vehicle_ambient_light_name(g_shmem->vehicle_state.ambient_light));
    uart_put_str("\r");
    uart_put_str("| hazard=");
    uart_put_u32(g_shmem->vehicle_state.hazard);
    uart_put_str(" mode=");
    uart_put_str(light_vehicle_drive_mode_name(g_shmem->vehicle_state.drive_mode));
    uart_put_str(" allow=");
    uart_put_hex8((uint8_t)g_shmem->allow_flags);
    uart_put_str(" contract=");
    uart_put_str(shared_state_contract_name()[0] == 'O' ? DEMO_ANSI_GREEN : DEMO_ANSI_RED);
    uart_put_str(shared_state_contract_name());
    uart_put_str(DEMO_ANSI_RESET);
    uart_put_str("\r");
    uart_put_str("| lamps  ");
    uart_put_lamp_state("LOW", g_shmem->target_output.low_beam_on);
    uart_put_lamp_state("HIGH", g_shmem->target_output.high_beam_on);
    uart_put_lamp_state("LEFT", g_shmem->target_output.left_turn_on);
    uart_put_lamp_state("RIGHT", g_shmem->target_output.right_turn_on);
    uart_put_lamp_state("MARKER", g_shmem->target_output.marker_on);
    uart_put_lamp_state("BRAKE", g_shmem->target_output.brake_on);
    uart_put_str("\r");
    uart_put_str("| hint   L/l H/h Z/z Y/y P/p B/b = lights; speed=80, gear=drive, etc.   |\r");
    uart_put_str("|        !/# inject faults; C clears or advances recovery; ? shows this. |\r");
    uart_put_str(DEMO_ANSI_BOLD);
    uart_put_str("+--------------------------------------------------------------------------+");
    uart_put_str(DEMO_ANSI_RESET);
    uart_put_str("\r");
}

static const char *shared_state_contract_name(void) {
    if (g_shmem->layout_version != LIGHT_SHARED_STATE_LAYOUT_CURRENT) {
        return "LAYOUT_MISMATCH";
    }
    if (g_shmem->fault_mode > (uint8_t)LIGHT_FAULT_MODE_SAFE_MODE) {
        return "FAULT_MODE";
    }
    if (g_shmem->fault_lifecycle > (uint8_t)LIGHT_FAULT_LIFECYCLE_RECOVERING) {
        return "FAULT_LIFECYCLE";
    }
    if (g_shmem->fault_recovery_ticks > light_fault_recovery_window_ticks()) {
        return "FAULT_RECOVERY";
    }
    if (g_shmem->fault_recovery_elapsed_ms > light_fault_recovery_window_ms()) {
        return "FAULT_RECOVERY";
    }
    if ((g_shmem->active_fault_mask & (uint8_t)~0x0fU) != 0U) {
        return "FAULT_MASK";
    }

    return "OK";
}

/**
 * @brief 组件初始化入口函数
 * @details Microkit框架初始化阶段调用，完成UART初始化并打印启动日志
 * @param 无
 * @return 无
 */
void init(void) {
    // First we initialise the UART device, which will write to the
    // device's hardware registers. Which means we need access to
    // the UART device.
    
    uart_init();
    light_transport_parser_init(&g_transport_parser);
    g_shmem = (light_shmem_t *)shared_memory_base_vaddr;
    
    LOG_INFO("CMD_INIT module=commandin status=ready irq_channel=%d out_channel=%d",
             LIGHT_CH_COMMANDIN_UART_IRQ, LIGHT_CH_COMMANDIN_TO_SCHEDULER);
    LOG_INFO("COMMAND_IN SERVER IS RUNNING");
    LOG_INFO("DEMO_READY commands='L/l H/h Z/z Y/y P/p B/b ? C ! # speed=80 gear=drive ambient=night hazard=1 mode=city'");
    uart_put_str("\r");
    uart_put_str(DEMO_ANSI_CYAN);
    uart_put_str("[READY] LightDemo v2.0 console. Type ? for a colored status panel.\r");
    uart_put_str(DEMO_ANSI_RESET);
}

static void write_transport_message(light_transport_message_t message) {
    *(light_transport_message_t *)input_buffer = message;
}

static void dispatch_transport_message(light_transport_message_t message) {
    light_transport_route_t route = light_transport_route_for_message(message);
    light_contract_check_t contract =
        light_contract_check_transport_message(message, (light_transport_msg_type_t)message.type);

    if (contract.status != LIGHT_CONTRACT_OK) {
        LOG_ERROR("CMD_CONTRACT_REJECT reason=%s expected=%u actual=%u type=%u len=%u version=%u",
                  light_contract_status_name(contract.status),
                  (unsigned int)contract.expected,
                  (unsigned int)contract.actual,
                  (unsigned int)message.type,
                  (unsigned int)message.len,
                  (unsigned int)message.version);
        return;
    }

    write_transport_message(message);

    switch (route) {
        case LIGHT_TRANSPORT_ROUTE_SCHEDULER:
            microkit_notify(LIGHT_CH_COMMANDIN_TO_SCHEDULER);
            break;
        case LIGHT_TRANSPORT_ROUTE_VEHICLE_STATE:
            microkit_notify(LIGHT_CH_COMMANDIN_TO_VEHICLE_STATE);
            break;
        case LIGHT_TRANSPORT_ROUTE_FAULT_MGMT:
            microkit_notify(LIGHT_CH_COMMANDIN_TO_FAULTMG);
            break;
        case LIGHT_TRANSPORT_ROUTE_COMMANDIN:
            break;
        case LIGHT_TRANSPORT_ROUTE_NONE:
        default:
            break;
    }
}

static void emit_status_snapshot(void) {
    uart_put_str("STATUS_SNAPSHOT fault=");
    uart_put_str(light_fault_mode_name((fault_mode_t)g_shmem->fault_mode));
    uart_put_str(" lifecycle=");
    uart_put_str(light_fault_lifecycle_name((light_fault_lifecycle_t)g_shmem->fault_lifecycle));
    uart_put_str(" recovery_ticks=");
    uart_put_u32(g_shmem->fault_recovery_ticks);
    uart_put_char('/');
    uart_put_u32(light_fault_recovery_window_ticks());
    uart_put_str(" recovery_elapsed_ms=");
    uart_put_u32(g_shmem->fault_recovery_elapsed_ms);
    uart_put_char('/');
    uart_put_u32(light_fault_recovery_window_ms());
    uart_put_str(" active_faults=");
    uart_put_hex8(g_shmem->active_fault_mask);
    uart_put_str(" speed=");
    uart_put_u32(g_shmem->vehicle_state.speed_kph);
    uart_put_str(" ignition=");
    uart_put_u32(g_shmem->vehicle_state.ignition_on);
    uart_put_str(" brake_pedal=");
    uart_put_u32(g_shmem->vehicle_state.brake_pedal);
    uart_put_str(" gear=");
    uart_put_str(light_vehicle_gear_name(g_shmem->vehicle_state.gear));
    uart_put_str(" ambient=");
    uart_put_str(light_vehicle_ambient_light_name(g_shmem->vehicle_state.ambient_light));
    uart_put_str(" hazard=");
    uart_put_u32(g_shmem->vehicle_state.hazard);
    uart_put_str(" drive_mode=");
    uart_put_str(light_vehicle_drive_mode_name(g_shmem->vehicle_state.drive_mode));
    uart_put_str(" target[low=");
    uart_put_u32(g_shmem->target_output.low_beam_on);
    uart_put_str(" high=");
    uart_put_u32(g_shmem->target_output.high_beam_on);
    uart_put_str(" left=");
    uart_put_u32(g_shmem->target_output.left_turn_on);
    uart_put_str(" right=");
    uart_put_u32(g_shmem->target_output.right_turn_on);
    uart_put_str(" marker=");
    uart_put_u32(g_shmem->target_output.marker_on);
    uart_put_str(" brake=");
    uart_put_u32(g_shmem->target_output.brake_on);
    uart_put_str("] allow=");
    uart_put_hex8((uint8_t)g_shmem->allow_flags);
    uart_put_str(" last_fault=");
    uart_put_hex8(g_shmem->last_fault_code);
    uart_put_str(" last_fault_name=");
    uart_put_str(light_fault_code_name(g_shmem->last_fault_code));
    uart_put_str(" total_faults=");
    uart_put_u32(g_shmem->total_fault_count);
    uart_put_str(" layout=");
    uart_put_u32(g_shmem->layout_version);
    uart_put_str(" contract=");
    uart_put_str(shared_state_contract_name());
    uart_put_str("\r");
}

/**
 * @brief Microkit通道通知处理函数
 * @details 处理UART中断通道通知，读取字符、清除中断标志、解析并派发transport消息。
 * @param channel 触发通知的通道编号
 * @return 无
 * @note 仅处理LIGHT_CH_COMMANDIN_UART_IRQ通道，其他通道打印错误提示
 */
void notified(microkit_channel channel) {

    if (channel == LIGHT_CH_COMMANDIN_UART_IRQ) {
        // 1. 调用uart_get_char()获取键盘输入的字符
        int ch = uart_get_char();
    
        // 3. 调用uart_handle_irq()清除UART硬件的中断标志
        uart_handle_irq();
    
        // 4. 调用microkit_irq_ack()告知seL4中断已处理，可接收下一个中断
        microkit_irq_ack(channel);

        
        {
            light_transport_message_t message;
            light_transport_feed_status_t status;

            status = light_transport_parser_feed_char(&g_transport_parser, ch, &message);
            if (status == LIGHT_TRANSPORT_FEED_MESSAGE_READY) {
                light_transport_route_t route = light_transport_route_for_message(message);
                if (message.type == LIGHT_TRANSPORT_MSG_LIGHT_CMD) {
                    LOG_INFO("CMD_MSG type=light_cmd cmd=0x%02x",
                             (unsigned int)message.payload.light_cmd);
                    LOG_INFO("DEMO_FLOW input=light_cmd name=%s route=%s",
                             demo_light_cmd_name(message.payload.light_cmd),
                             demo_route_name(route));
                } else if (message.type == LIGHT_TRANSPORT_MSG_VEHICLE_STATE_UPDATE) {
                    LOG_INFO("CMD_MSG type=vehicle_state field=%u value=%u",
                             (unsigned int)message.payload.vehicle_state_update.field,
                             (unsigned int)message.payload.vehicle_state_update.value);
                    LOG_INFO("DEMO_FLOW input=vehicle_state field=%s value=%u route=%s",
                             demo_vehicle_field_name(message.payload.vehicle_state_update.field),
                             (unsigned int)message.payload.vehicle_state_update.value,
                             demo_route_name(route));
                } else if (message.type == LIGHT_TRANSPORT_MSG_FAULT_INJECT) {
                    LOG_INFO("CMD_MSG type=fault_inject code=0x%02x",
                             (unsigned int)message.payload.fault_error_code);
                    LOG_INFO("DEMO_FLOW input=fault_inject code=%s route=%s",
                             light_fault_code_name(message.payload.fault_error_code),
                             demo_route_name(route));
                } else if (message.type == LIGHT_TRANSPORT_MSG_FAULT_CLEAR) {
                    LOG_INFO("CMD_MSG type=fault_clear scope=%u",
                             (unsigned int)message.payload.fault_clear_scope);
                    LOG_INFO("DEMO_FLOW input=fault_clear action=clear_or_recover route=%s",
                             demo_route_name(route));
                } else if (message.type == LIGHT_TRANSPORT_MSG_QUERY) {
                    LOG_INFO("CMD_MSG type=query_status");
                    LOG_INFO("DEMO_FLOW input=query route=%s", demo_route_name(route));
                }
                emit_demo_input_line(message, route);

                if (route == LIGHT_TRANSPORT_ROUTE_COMMANDIN) {
                    emit_status_snapshot();
                    emit_demo_status_panel();
                } else {
                    dispatch_transport_message(message);
                }
            } else if (status == LIGHT_TRANSPORT_FEED_ERROR) {
                LOG_ERROR("transport parse error\n");
                LOG_ERROR("DEMO_FLOW input=parse_error hint='Use L/l H/h Z/z Y/y P/p B/b ? C ! # or key=value plus Enter'");
            }
        }

    }
    else{
        LOG_ERROR("无法解析的通道信号\n");
    }
    
}
