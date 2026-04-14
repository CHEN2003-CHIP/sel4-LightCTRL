/**
 * @file uart_cmd_handler.c
 * @brief 车灯控制系统-UART命令处理组件
 * @details 基于seL4+Microkit框架实现UART串口通信，解析键盘输入的车灯控制指令，
 *          将指令转换为标准化操作码后通过共享缓冲区发送至lightctl组件执行
 * @author USTC-CHEN
 * @date 2025-12-05
 * @note 仅支持近光灯、远光灯、左右转向灯的开关控制指令解析，指令采用字符编码方式
 */


#include <stdint.h>
#include <stdbool.h>
#include <microkit.h>
#include "printf.h"
#include <stdatomic.h>
#include <stdio.h>
#include "logger.h"
#include "light_fault_mode.h"
#include "light_protocol.h"
#include "light_status_snapshot.h"
#include "light_transport.h"


/*
---------COMMAND---------
               H   L    OP
近光灯开启	    0	1	0x01    L
近光灯关闭	    0	0	0x00    l
远光灯开启	    1	1	0x11    H
远光灯关闭	    1	0	0x10    h
左转向灯开启	2	1	0x21    Z
左转向灯关闭	2	0	0x20    z
右转向灯开启	3	1	0x31    Y
右转向灯关闭	3	0	0x30    y

*/


// This variable will have the address of the UART device
/**
 * @var uart_base_vaddr
 * @brief UART设备硬件寄存器基地址
 * @note 由Microkit系统描述文件配置，指向UART外设的虚拟地址
 */
uintptr_t uart_base_vaddr;

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

#define LIGHTCTL_CHANNEL 3
#define VEHICLE_STATE_CHANNEL 17
#define UARTIRP_CHANNEL 0
#define TEST_FAULT_CHANNEL 11

#if LIGHT_ENABLE_TEST_HOOKS
#define TEST_FAULT_MODE_CONFLICT '!'
#define TEST_FAULT_HW_STATE '#'
#endif

static light_transport_parser_t g_transport_parser;
static light_shmem_t *g_shmem = NULL;

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
void uart_put_str(char *str) {
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
             UARTIRP_CHANNEL, LIGHTCTL_CHANNEL);
    LOG_INFO("COMMAND_IN SERVER IS RUNNING");
}

static void write_transport_message(light_transport_message_t message) {
    *(light_transport_message_t *)input_buffer = message;
}

static void dispatch_transport_message(light_transport_message_t message) {
    light_transport_route_t route = light_transport_route_for_message(message);

    write_transport_message(message);

    switch (route) {
        case LIGHT_TRANSPORT_ROUTE_SCHEDULER:
            microkit_notify(LIGHTCTL_CHANNEL);
            break;
        case LIGHT_TRANSPORT_ROUTE_VEHICLE_STATE:
            microkit_notify(VEHICLE_STATE_CHANNEL);
            break;
        case LIGHT_TRANSPORT_ROUTE_FAULT_MGMT:
            microkit_notify(TEST_FAULT_CHANNEL);
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
    uart_put_str((char *)light_fault_mode_name((fault_mode_t)g_shmem->fault_mode));
    uart_put_str(" lifecycle=");
    uart_put_str((char *)light_fault_lifecycle_name((light_fault_lifecycle_t)g_shmem->fault_lifecycle));
    uart_put_str(" recovery_ticks=");
    uart_put_u32(g_shmem->fault_recovery_ticks);
    uart_put_char('/');
    uart_put_u32(light_fault_recovery_window_ticks());
    uart_put_str(" active_faults=");
    uart_put_hex8(g_shmem->active_fault_mask);
    uart_put_str(" speed=");
    uart_put_u32(g_shmem->vehicle_state.speed_kph);
    uart_put_str(" ignition=");
    uart_put_u32(g_shmem->vehicle_state.ignition_on);
    uart_put_str(" brake_pedal=");
    uart_put_u32(g_shmem->vehicle_state.brake_pedal);
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
    uart_put_str(" total_faults=");
    uart_put_u32(g_shmem->total_fault_count);
    uart_put_str("\r");
}

/**
 * @brief 转换指令字符为标准化操作码并发送
 * @details 将键盘输入的指令字符映射为16进制操作码，调用write_command发送
 * @param ch 输入的指令字符（如L/l/H/h等）
 * @return 无
 * @note 无效字符会打印错误日志并返回，不发送指令
 */
/**
 * @brief Microkit通道通知处理函数
 * @details 处理UART中断通道通知，读取指令字符、清除中断标志、解析并发送车灯控制指令
 * @param channel 触发通知的通道编号
 * @return 无
 * @note 仅处理UARTIRP_CHANNEL通道，其他通道打印错误提示
 */
void notified(microkit_channel channel) {

    if (channel == UARTIRP_CHANNEL) {
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
                if (message.type == LIGHT_TRANSPORT_MSG_LIGHT_CMD) {
                    LOG_INFO("CMD_MSG type=light_cmd cmd=0x%02x",
                             (unsigned int)message.payload.light_cmd);
                } else if (message.type == LIGHT_TRANSPORT_MSG_VEHICLE_STATE_UPDATE) {
                    LOG_INFO("CMD_MSG type=vehicle_state field=%u value=%u",
                             (unsigned int)message.payload.vehicle_state_update.field,
                             (unsigned int)message.payload.vehicle_state_update.value);
                } else if (message.type == LIGHT_TRANSPORT_MSG_FAULT_INJECT) {
                    LOG_INFO("CMD_MSG type=fault_inject code=0x%02x",
                             (unsigned int)message.payload.fault_error_code);
                } else if (message.type == LIGHT_TRANSPORT_MSG_FAULT_CLEAR) {
                    LOG_INFO("CMD_MSG type=fault_clear scope=%u",
                             (unsigned int)message.payload.fault_clear_scope);
                } else if (message.type == LIGHT_TRANSPORT_MSG_QUERY) {
                    LOG_INFO("CMD_MSG type=query_status");
                }

                if (light_transport_route_for_message(message) == LIGHT_TRANSPORT_ROUTE_COMMANDIN) {
                    emit_status_snapshot();
                } else {
                    dispatch_transport_message(message);
                }
            } else if (status == LIGHT_TRANSPORT_FEED_ERROR) {
                LOG_ERROR("transport parse error\n");
            }
        }

    }
    else{
        LOG_ERROR("无法解析的通道信号\n");
    }
    
}
