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
#include "logger.h"
#include "light_command_codec.h"
#include "light_protocol.h"


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

static char g_vehicle_line[LIGHT_COMMAND_LINE_MAX];
static uint8_t g_vehicle_line_len = 0;

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
    g_vehicle_line_len = 0;
    
    LOG_INFO("CMD_INIT module=commandin status=ready irq_channel=%d out_channel=%d",
             UARTIRP_CHANNEL, LIGHTCTL_CHANNEL);
    LOG_INFO("COMMAND_IN SERVER IS RUNNING");
}

static void write_command_to_channel(int cm, microkit_channel channel){
    char* inputbuf=(char*)input_buffer;
    *(uint8_t*)inputbuf=cm;
    microkit_notify(channel);
}

static void write_vehicle_request(light_vehicle_state_request_t request) {
    *(light_vehicle_state_request_t *)input_buffer = request;
    microkit_notify(VEHICLE_STATE_CHANNEL);
}

#if LIGHT_ENABLE_TEST_HOOKS
static bool try_inject_test_fault(int ch) {
    uint8_t error_code = 0;

    switch (ch) {
        case TEST_FAULT_MODE_CONFLICT:
            error_code = LIGHT_ERR_MODE_CONFLICT;
            break;
        case TEST_FAULT_HW_STATE:
            error_code = LIGHT_ERR_HW_STATE_ERR;
            break;
        default:
            return false;
    }

    LOG_INFO("CMD_TEST_FAULT char=%c code=0x%02x channel=%d",
             ch,
             error_code,
             TEST_FAULT_CHANNEL);
    *(volatile uint8_t *)input_buffer = error_code;
    microkit_notify(TEST_FAULT_CHANNEL);
    return true;
}
#endif

/**
 * @brief 转换指令字符为标准化操作码并发送
 * @details 将键盘输入的指令字符映射为16进制操作码，调用write_command发送
 * @param ch 输入的指令字符（如L/l/H/h等）
 * @return 无
 * @note 无效字符会打印错误日志并返回，不发送指令
 */
void send_command(int ch)
{
    uint8_t operation_num = LIGHT_UART_CMD_INVALID;

#if LIGHT_ENABLE_TEST_HOOKS
    if (try_inject_test_fault(ch)) {
        return;
    }
#endif

    if (!light_command_decode_char(ch, &operation_num)) {
        LOG_ERROR("error operation num\n");
        return;
    }

    LOG_INFO("CMD_RX char=%c opcode=0x%02x target=%d", ch, operation_num, LIGHTCTL_CHANNEL);
    write_command_to_channel(operation_num, LIGHTCTL_CHANNEL);
}

static void reset_vehicle_line(void) {
    g_vehicle_line_len = 0;
    g_vehicle_line[0] = '\0';
}

static void flush_vehicle_line(void) {
    light_vehicle_state_request_t request;

    g_vehicle_line[g_vehicle_line_len] = '\0';
    if (!light_vehicle_state_parse_line(g_vehicle_line, &request)) {
        LOG_ERROR("invalid vehicle_state command: %s\n", g_vehicle_line);
        reset_vehicle_line();
        return;
    }

    LOG_INFO("CMD_VEHICLE line=%s field=%u value=%u target=%d",
             g_vehicle_line,
             (unsigned int)request.field,
             (unsigned int)request.value,
             VEHICLE_STATE_CHANNEL);
    write_vehicle_request(request);
    reset_vehicle_line();
}

static bool buffer_vehicle_line_char(int ch) {
    if (g_vehicle_line_len + 1U >= LIGHT_COMMAND_LINE_MAX) {
        LOG_ERROR("vehicle_state command too long\n");
        reset_vehicle_line();
        return false;
    }

    g_vehicle_line[g_vehicle_line_len++] = (char)ch;
    g_vehicle_line[g_vehicle_line_len] = '\0';
    return true;
}


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

        
        if (ch == '\r') {
            if (g_vehicle_line_len != 0U) {
                flush_vehicle_line();
            }
            return;
        }

        if (g_vehicle_line_len == 0U) {
            uint8_t op = LIGHT_UART_CMD_INVALID;

            if (light_command_decode_char(ch, &op)) {
                send_command(ch);
                return;
            }
        }

        buffer_vehicle_line_char(ch);

    }
    else{
        LOG_ERROR("无法解析的通道信号\n");
    }
    
}
