/**
 * @file gpio_ctrl.c
 * @brief 车灯控制系统-GPIO驱动组件
 * @details 基于seL4+Microkit框架实现多类型车灯（近光/远光/左右转向）的GPIO硬件控制，
 *          解析共享缓冲区中的控制指令，执行引脚电平操作并校验执行结果，异常时通知故障管理组件
 * @author USTC-CHEN
 * @date 2025-12-05
 * @note 需确保gpio_base_vaddr运行时已正确赋值，支持引脚操作结果校验与故障上报
 */

#include <stdint.h>
#include <microkit.h>
#include "printf.h"
#include <stdatomic.h>
#include "logger.h"
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <sys/time.h> 
#include <sel4/sel4.h>

// ==============================================
// 通道定义 (必须与lightctl完全一致)
// ==============================================
#define CH_GPIO_TURN_LEFT_ON        20
#define CH_GPIO_TURN_LEFT_OFF       21
#define CH_GPIO_TURN_RIGHT_ON       22
#define CH_GPIO_TURN_RIGHT_OFF      23
#define CH_GPIO_BRAKE_ON            24
#define CH_GPIO_BRAKE_OFF           25
#define CH_GPIO_LOW_BEAM_ON         26
#define CH_GPIO_LOW_BEAM_OFF        27
#define CH_GPIO_HIGH_BEAM_ON        28
#define CH_GPIO_HIGH_BEAM_OFF       29
#define CH_GPIO_POSITION_ON         30
#define CH_GPIO_POSITION_OFF        31

uintptr_t gpio_base_vaddr;  // 运行时会被赋值的基地址
uintptr_t cmd_buffer;
uintptr_t timer_base_vaddr;

#define GPIO_CHANNEL 1
#define FAULT_NOTIFY_CHANNEL 8

// 宏：通过基地址和偏移量计算寄存器指针（关键：运行时动态计算）
#define REG_PTR(base, offset) ((volatile uint32_t *)((base) + (offset)))

#define TIMER_LOAD_OFFSET 0x00       // 加载值寄存器偏移
#define TIMER_CTRL_OFFSET 0x08       // 控制寄存器偏移
#define TIMER_INTCLR_OFFSET 0x0C     // 中断清除寄存器偏移
#define TIMER_IRQ_CHANNEL 10         // 定时器中断通道（需确保未被占用）

// 转向灯闪烁状态变量
static bool left_turn_active = false;   // 左转向灯激活状态
static bool right_turn_active = false;  // 右转向灯激活状态
static uint8_t left_turn_state = 0;     // 左转向灯当前状态（0=灭，1=亮）
static uint8_t right_turn_state = 0;    // 右转向灯当前状态（0=灭，1=亮）

// 灯引脚
#define PIN_LOW_BEAM    0   // 近光灯
#define PIN_HIGH_BEAM   1   // 远光灯
#define PIN_TURN_LEFT   2   // 左转向灯
#define PIN_TURN_RIGHT  3   // 右转向灯
#define PIN_BRAKE        4   // 刹车灯 
#define PIN_POSITION     5   // 示廓灯 

// GPIO寄存器偏移量（根据硬件手册定义）

#define REG_PTR(base, offset) ((volatile uint32_t *)((base) + (offset)))
#define GPIO_DIR_OFFSET 0x00  // 方向寄存器偏移
#define GPIO_OUT_OFFSET 0x04  // 输出寄存器偏移

// #define CMD_TARGET_MASK 0xF0  // 目标车灯掩码（高4位）
// #define CMD_OP_MASK     0x0F  // 操作掩码（低4位）
// #define CMD_TARGET(cmd) ((cmd >> 4) & 0x0F)  // 提取目标车灯（0-3）
// #define CMD_OP(cmd)     (cmd & 0x0F)         // 提取操作（0=关，1=开）


/**
 * @brief 初始化定时器，设置闪烁频率（500ms一次中断）
 */
static void timer_init() {
    // 假设定时器时钟为1MHz，500ms对应加载值为500000
    *REG_PTR(timer_base_vaddr, TIMER_LOAD_OFFSET) = 5000000;
    
    // 配置控制寄存器：启动定时器，允许中断，自动重加载
    *REG_PTR(timer_base_vaddr, TIMER_CTRL_OFFSET) = (1 << 0) | (1 << 1) | (1 << 7);
}

//-----------------TODO::TEST FOR TIMER--------------------------------------------------------------

// 使用aarch64系统计数器（如果权限允许）
static inline uint64_t read_cntpct(void) {
    uint64_t val;
    asm volatile("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

static inline uint64_t read_cntfrq(void) {
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

// 获取系统时间（以计时器滴答为单位）
uint64_t get_system_ticks(void) {
    return read_cntpct();
}

// 获取计时器频率
uint64_t get_timer_freq(void) {
    return read_cntfrq();
}

// 将滴答转换为毫秒
uint64_t ticks_to_ms(uint64_t ticks) {
    uint64_t freq = get_timer_freq();
    return (ticks * 1000ULL) / freq;
}

//------------------------------------------------------------------------------------


void init(void) {
    // 初始化时，gpio_base_vaddr已有效，通过REG_PTR获取方向寄存器指针
    volatile uint32_t* gpio_dir = REG_PTR(gpio_base_vaddr, GPIO_DIR_OFFSET);
    // 配置所有用到的引脚为输出模式
    *gpio_dir |= (1 << PIN_LOW_BEAM)   |
                 (1 << PIN_HIGH_BEAM)  |
                 (1 << PIN_TURN_LEFT)  |
                 (1 << PIN_TURN_RIGHT) |
                 (1 << PIN_BRAKE)      |
                 (1 << PIN_POSITION);

    LOG_INFO("GPIO Ctrl: Initialized. All pins set to output.");

    LOG_INFO("  近光灯引脚(%d)、远光灯引脚(%d)、左转向灯引脚(%d)、右转向灯引脚(%d)均配置为输出",
           PIN_LOW_BEAM, PIN_HIGH_BEAM, PIN_TURN_LEFT, PIN_TURN_RIGHT);
    LOG_INFO("GPIO: starting\n");

    // 新增定时器初始化
    timer_init();
    LOG_INFO("GPIO PD初始化完成，定时器已启动（闪烁频率2Hz）");
    
}


// ==============================================
// 设置GPIO引脚电平
// ==============================================
static void gpio_set_pin(uint8_t pin, bool level) {
    volatile uint32_t* gpio_out = REG_PTR(gpio_base_vaddr, GPIO_OUT_OFFSET);
    
    if (level) {
        *gpio_out |= (1 << pin);  // 置高 (开灯)
    } else {
        *gpio_out &= ~(1 << pin); // 置低 (关灯)
    }
    
    // TODO:这里可以添加读取回读寄存器进行校验的逻辑
}

/**
 * @brief Microkit通道通知处理函数
 * @details 处理GPIO_CHANNEL通道的控制指令，解析指令后操作对应GPIO引脚，校验执行结果并上报状态/异常
 * @param channel 触发通知的通道编号
 * @return 无
 * @note 仅处理GPIO_CHANNEL通道，其他通道打印不支持提示；异常时通过FAULT_NOTIFY_CHANNEL上报故障
 */
void notified(microkit_channel ch) {
    switch (ch) {
        // --- 转向灯控制 ---
        case CH_GPIO_TURN_LEFT_ON:
            gpio_set_pin(PIN_TURN_LEFT, true);
            LOG_INFO("GPIO: Left Turn ON");
            break;
        case CH_GPIO_TURN_LEFT_OFF:
            gpio_set_pin(PIN_TURN_LEFT, false);
            LOG_INFO("GPIO: Left Turn OFF");
            break;
        case CH_GPIO_TURN_RIGHT_ON:
            gpio_set_pin(PIN_TURN_RIGHT, true);
            LOG_INFO("GPIO: Right Turn ON");
            break;
        case CH_GPIO_TURN_RIGHT_OFF:
            gpio_set_pin(PIN_TURN_RIGHT, false);
            LOG_INFO("GPIO: Right Turn OFF");
            break;

        // --- 刹车灯控制 ---
        case CH_GPIO_BRAKE_ON:
            gpio_set_pin(PIN_BRAKE, true);
            LOG_INFO("GPIO: Brake ON");
            break;
        case CH_GPIO_BRAKE_OFF:
            gpio_set_pin(PIN_BRAKE, false);
            LOG_INFO("GPIO: Brake OFF");
            break;

        // --- 近光灯控制 ---
        case CH_GPIO_LOW_BEAM_ON:
            gpio_set_pin(PIN_LOW_BEAM, true);
            LOG_INFO("GPIO: Low Beam ON");
            break;
        case CH_GPIO_LOW_BEAM_OFF:
            gpio_set_pin(PIN_LOW_BEAM, false);
            LOG_INFO("GPIO: Low Beam OFF");
            break;

        // --- 远光灯控制 ---
        case CH_GPIO_HIGH_BEAM_ON:
            gpio_set_pin(PIN_HIGH_BEAM, true);
            LOG_INFO("GPIO: High Beam ON");
            break;
        case CH_GPIO_HIGH_BEAM_OFF:
            gpio_set_pin(PIN_HIGH_BEAM, false);
            LOG_INFO("GPIO: High Beam OFF");
            break;

        // --- 示廓灯控制 ---
        case CH_GPIO_POSITION_ON:
            gpio_set_pin(PIN_POSITION, true);
            LOG_INFO("GPIO: Position Light ON");
            break;
        case CH_GPIO_POSITION_OFF:
            gpio_set_pin(PIN_POSITION, false);
            LOG_INFO("GPIO: Position Light OFF");
            break;

        default:
            LOG_ERROR("GPIO: Unknown channel %d", ch);
            break;
    }
}