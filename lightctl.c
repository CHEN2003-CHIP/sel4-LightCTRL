/**
 * @file lightctl.c
 * @brief 车灯控制系统-核心控制组件
 * @details 基于seL4+Microkit框架实现车灯控制指令的转发与状态响应，接收commandin组件下发的控制指令并转发至GPIO组件，
 *          同时处理GPIO组件的操作结果反馈、故障管理组件的异常通知
 * @author USTC-CHEN
 * @date 2025-12-05
 * @note 依赖共享缓冲区实现跨组件指令传递，支持多通道状态响应与故障处理
 */

#include <stdint.h>
#include <stdbool.h>
#include <microkit.h>
#include "printf.h"
#include "wordle.h"
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include "logger.h"


/* Microkit通信通道定义 */
#define LIGHTCTL_CHANNEL           2           /* 与GPIO组件通信的通道号（指令下发/操作结果接收） */
#define LIGHTCTL_COMMANDIN_CHANNEL 4           /* 接收commandin组件控制指令的通道号 */
#define LIGHTCTL_FAULTMG_CHANNEL   6           /* 接收故障管理组件异常通知的通道号 */

// 通道ID：调度器→灯光控制（允许执行通知）
#define CH_SCHEDULER_ALLOW     10

// 通道ID：灯光控制→GPIO模块（每个操作一个独立通道）
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
// 通道ID：灯光控制→错误处理模块
#define CH_ERROR_REPORT        6

// 错误码定义（与错误处理模块约定）
#define ERR_SPEED_LIMIT        0x01 // 车速超限
#define ERR_MODE_CONFLICT      0x02 // 模式冲突
#define ERR_INVALID_CMD        0x03 // 无效指令
#define ERR_HW_STATE_ERR       0x04 // 硬件状态不一致


uintptr_t cmd_buffer;
uintptr_t input_buffer;  // 由系统描述文件的setvar_vaddr自动赋值
uintptr_t shared_memory_base_vaddr;//共享的信号结构体，具体定义见scheduler模块

// ==============================================
// 共享内存定义 
// ==============================================
#define FLAG_ALLOW_BRAKE       (1UL << 0)
#define FLAG_ALLOW_TURN_LEFT   (1UL << 1)
#define FLAG_ALLOW_TURN_RIGHT  (1UL << 2)
#define FLAG_ALLOW_LOW_BEAM    (1UL << 3)
#define FLAG_ALLOW_HIGH_BEAM   (1UL << 4)
#define FLAG_ALLOW_POSITION    (1UL << 5)

#define IS_FLAG_SET(flags, mask)  (((flags) & (mask)) != 0)

typedef struct {
    volatile uint8_t  uart_cmd;
    volatile uint32_t allow_flags; // 【修改】替换位域
    volatile uint8_t  turn_switch_pos;
    volatile uint8_t  beam_switch_pos;
    volatile uint16_t vehicle_speed;
} light_shmem_t;


// 共享内存指针
static light_shmem_t * g_shmem = NULL;

// 记录上一次的操作状态，用于状态一致性校验
static uint8_t g_last_turn_state = 0;    // 0=关，1=左开，2=右开
static uint8_t g_last_beam_state = 0;    // 0=关，1=近光开，2=远光开
static uint8_t g_last_brake_state = 0;   // 0=关，1=开
static uint8_t g_last_position_state = 1;// 0=关，1=开（默认开）

// ==============================================
// 车速阈值校验
// ==============================================
static bool check_speed_limit(microkit_channel ch) {
    if ((ch == CH_GPIO_TURN_LEFT_ON || ch == CH_GPIO_TURN_RIGHT_ON) 
        && g_shmem->vehicle_speed > 120) {
        LOG_INFO("LightCtrl: Speed limit exceeded (>120km/h), turn light denied\n");
        microkit_mr_set(0, ERR_SPEED_LIMIT);
        microkit_notify(CH_ERROR_REPORT);
        return false;
    }
    if (ch == CH_GPIO_HIGH_BEAM_ON && g_shmem->vehicle_speed < 10) {
        LOG_INFO("LightCtrl: Speed too low (<10km/h), high beam denied\n");
        microkit_mr_set(0, ERR_SPEED_LIMIT);
        microkit_notify(CH_ERROR_REPORT);
        return false;
    }
    return true;
}

// ==============================================
// 模式互锁校验
// ==============================================
static bool check_mode_conflict(microkit_channel ch) {
    if (ch == CH_GPIO_LOW_BEAM_OFF && g_last_beam_state == 2) {
        LOG_INFO("LightCtrl: Mode conflict, can't turn off low beam when high beam is on\n");
        microkit_mr_set(0, ERR_MODE_CONFLICT);
        microkit_notify(CH_ERROR_REPORT);
        return false;
    }
    if ((ch == CH_GPIO_TURN_LEFT_ON || ch == CH_GPIO_TURN_RIGHT_ON) 
        && g_last_brake_state == 1) {
        LOG_INFO("LightCtrl: Mode conflict, brake light active, turn light denied\n");
        microkit_mr_set(0, ERR_MODE_CONFLICT);
        microkit_notify(CH_ERROR_REPORT);
        return false;
    }
    return true;
}


// ==============================================
// 辅助函数：触发GPIO模块操作
// ==============================================
static void trigger_gpio_operation(microkit_channel ch) {
    // 异步通知GPIO模块执行操作
    microkit_notify(ch);
    LOG_INFO("LightCtrl: Trigger GPIO on channel %d\n", ch);
}


void init(void) {
    g_shmem = (light_shmem_t *)shared_memory_base_vaddr;
    // 可选：显式检查地址是否有效（防止SDF配置错误）
    if (g_shmem == NULL) {
        microkit_dbg_puts("LightCtrl ERROR: Shared memory not mapped!\n");
        // 这里可以陷入死循环或停止，因为系统无法运行
        while(1);
    }

    // 初始化历史状态
    g_last_turn_state = 0;
    g_last_beam_state = 0;
    g_last_brake_state = 0;
    g_last_position_state = 1;

    // 初始化解调输出
    LOG_INFO("Light control module initialized\n");
}

// /**
//  * @brief 接收并转发车灯控制指令
//  * @details 从input_buffer读取commandin组件下发的控制指令，打印指令日志后写入cmd_buffer，
//  *          并通知GPIO组件处理该指令
//  * @param 无
//  * @return 无
//  * @note 指令为1字节格式（高4位=目标车灯，低4位=操作类型），直接透传至GPIO组件
//  */
// void recieve_command()
// {
//     uint8_t cmd = *(uint8_t*)input_buffer;
//     LOG_INFO("lightctl:收到信号码：%x",cmd);
//     LOG_INFO("转述进gpio 通信通道");

//     //SEND THE CMD
//     char* cmdbuf=(char*)cmd_buffer;
//     *(uint8_t*)cmdbuf=cmd;
//     microkit_notify(LIGHTCTL_CHANNEL);
    
// }

/**
 * @brief Microkit通道通知处理函数
 * @details 处理不同通道的通知事件，分别响应GPIO操作结果、commandin指令、故障管理异常通知
 * @param channel 触发通知的通道编号
 * @return 无
 * @note 仅处理预定义的三个通道，其他通道无响应动作
 */
// ==============================================
// 收到调度器通知时执行
// ==============================================
void notified(microkit_channel ch) {
    // 仅处理调度器的允许执行通知
    if (ch != CH_SCHEDULER_ALLOW) {
        LOG_INFO("LightCtrl: Unknown channel, ignore\n");
        microkit_mr_set(0, ERR_INVALID_CMD);
        microkit_notify(CH_ERROR_REPORT);
        return;
    }

    // ==========================================
    // 调试打印
    // ==========================================
    LOG_INFO("--- LightCtrl State Check ---");
    LOG_INFO("Shmem: brake=%d, left=%d, right=%d, low=%d, high=%d, pos=%d",
             IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_BRAKE),
             IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_TURN_LEFT),
             IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_TURN_RIGHT),
             IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_LOW_BEAM),
             IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_HIGH_BEAM),
             IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_POSITION));
    LOG_INFO("Internal: last_brake=%d, last_turn=%d, last_beam=%d",
             g_last_brake_state,
             g_last_turn_state,
             g_last_beam_state);
    // ==========================================


    LOG_INFO("GET SCHEDULER SIGANL");

    // ==========================================
    // 处理刹车灯逻辑
    // ==========================================
    if (IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_BRAKE)) {
        if (g_last_brake_state != 1) {
            if (check_mode_conflict(CH_GPIO_BRAKE_ON)) {
                trigger_gpio_operation(CH_GPIO_BRAKE_ON);
                g_last_brake_state = 1;
            }
        }
    } else {
        if (g_last_brake_state != 0) {
            trigger_gpio_operation(CH_GPIO_BRAKE_OFF);
            g_last_brake_state = 0;
        }
    }

    // ==========================================
    // 处理转向灯逻辑
    // ==========================================
    if (IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_TURN_LEFT)) {
        if (g_last_turn_state != 1) {
            if (check_speed_limit(CH_GPIO_TURN_LEFT_ON) && check_mode_conflict(CH_GPIO_TURN_LEFT_ON)) {
                trigger_gpio_operation(CH_GPIO_TURN_LEFT_ON);
                g_last_turn_state = 1;
            }
        }
    } else if (IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_TURN_RIGHT)) {
        if (g_last_turn_state != 2) {
            if (check_speed_limit(CH_GPIO_TURN_RIGHT_ON) && check_mode_conflict(CH_GPIO_TURN_RIGHT_ON)) {
                trigger_gpio_operation(CH_GPIO_TURN_RIGHT_ON);
                g_last_turn_state = 2;
            }
        }
    } else {
        if (g_last_turn_state != 0) {
            microkit_channel off_ch = (g_last_turn_state == 1) ? CH_GPIO_TURN_LEFT_OFF : CH_GPIO_TURN_RIGHT_OFF;
            trigger_gpio_operation(off_ch);
            g_last_turn_state = 0;
        }
    }

    // ==========================================
    // 处理远近光灯逻辑
    // ==========================================
    if (IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_LOW_BEAM)) {
        if (g_last_beam_state != 1) {
            if (check_mode_conflict(CH_GPIO_LOW_BEAM_ON)) {
                trigger_gpio_operation(CH_GPIO_LOW_BEAM_ON);
                g_last_beam_state = 1;
            }
        }
    } else if (IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_HIGH_BEAM)) {
        if (g_last_beam_state != 2) {
            if (check_speed_limit(CH_GPIO_HIGH_BEAM_ON) && check_mode_conflict(CH_GPIO_HIGH_BEAM_ON)) {
                trigger_gpio_operation(CH_GPIO_HIGH_BEAM_ON);
                g_last_beam_state = 2;
            }
        }
    } else {
        if (g_last_beam_state != 0) {
            if (g_last_beam_state == 2) {
                trigger_gpio_operation(CH_GPIO_HIGH_BEAM_OFF);
            }
            trigger_gpio_operation(CH_GPIO_LOW_BEAM_OFF);
            g_last_beam_state = 0;
        }
    }

    // ==========================================
    // 处理示廓灯逻辑
    // ==========================================
    if (IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_POSITION)) {
        if (g_last_position_state != 1) {
            trigger_gpio_operation(CH_GPIO_POSITION_ON);
            g_last_position_state = 1;
        }
    } else {
        if (g_last_position_state != 0) {
            trigger_gpio_operation(CH_GPIO_POSITION_OFF);
            g_last_position_state = 0;
        }
    }
}