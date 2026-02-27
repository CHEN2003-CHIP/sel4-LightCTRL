#include <stdint.h>
#include <stdbool.h>
#include <microkit.h>
#include <stddef.h>
#include"include/logger.h"

// ==============================================
// 1. 手动宏定义（与SDF严格一致，SDK 2.0.1无setvar_id）
// ==============================================
// 通道ID：UART命令模块→调度器（对应你原代码的LIGHTCTL_CHANNEL=3）
#define CH_UART_CMD            4
// 通道ID：调度器→灯光控制模块
#define CH_LIGHT_CONTROL_ALLOW 9

// 共享内存地址（SDF中配置的vaddr，与UART模块共享）
uintptr_t shared_memory_base_vaddr;
uintptr_t input_buffer;  // 由系统描述文件的setvar_vaddr自动赋值
// #define SHMEM_VADDR            0x40000000
// #define SHMEM_SIZE             0x1000  // 4KB

// ==============================================
// 2. 操作码定义（与你提供的uart_cmd_handler.c完全一致）
// ==============================================
#define UART_CMD_LOW_BEAM_OFF   0x00  // 'l'
#define UART_CMD_LOW_BEAM_ON    0x01  // 'L'
#define UART_CMD_HIGH_BEAM_OFF  0x10  // 'h'
#define UART_CMD_HIGH_BEAM_ON   0x11  // 'H'
#define UART_CMD_LEFT_TURN_OFF  0x20  // 'z'
#define UART_CMD_LEFT_TURN_ON   0x21  // 'Z'
#define UART_CMD_RIGHT_TURN_OFF 0x30  // 'y'
#define UART_CMD_RIGHT_TURN_ON  0x31  // 'Y'

// ==============================================
// 3. 共享内存数据结构（仅调度器可写状态位，UART模块可写cmd）
// ==============================================
typedef struct {
    // UART命令模块写入的操作码（调度器只读）
    volatile uint8_t  uart_cmd;

    // 全局允许状态位（仅调度器可写，1=允许，0=禁止）
    volatile uint32_t allow_brake    : 1;
    volatile uint32_t allow_turn_left : 1;
    volatile uint32_t allow_turn_right: 1;
    volatile uint32_t allow_low_beam  : 1;
    volatile uint32_t allow_high_beam : 1;
    volatile uint32_t allow_position  : 1;
    volatile uint32_t reserved        : 26;

    // 原始信号参数（预留，后续扩展用）
    volatile uint8_t  turn_switch_pos;  // 0=复位，1=左，2=右
    volatile uint8_t  beam_switch_pos;  // 0=复位，1=近光，2=远光
    volatile uint16_t vehicle_speed;     // 单位：km/h
} light_shmem_t;

// 共享内存指针（强制转换为SDF配置的vaddr）
// static light_shmem_t *const g_shmem = (light_shmem_t *const)shared_memory_base_vaddr;
static light_shmem_t * g_shmem = NULL;
// ==============================================
// 4. 强制入口点1：系统启动时调用（仅初始化一次）
// ==============================================
void init(void) {
    // 初始化UART操作码
    g_shmem=(light_shmem_t *)shared_memory_base_vaddr;
    g_shmem->uart_cmd = 0xFF; // 初始化为无效值

    // 初始化全局允许状态：默认仅允许示廓灯
    g_shmem->allow_brake    = 0;
    g_shmem->allow_turn_left = 0;
    g_shmem->allow_turn_right= 0;
    g_shmem->allow_low_beam  = 0;
    g_shmem->allow_high_beam = 0;
    g_shmem->allow_position  = 1;
    g_shmem->reserved        = 0;

    // 初始化原始信号参数
    g_shmem->turn_switch_pos = 0;
    g_shmem->beam_switch_pos = 0;
    g_shmem->vehicle_speed    = 0;
    LOG_INFO("g_shmem->allow_brake:%d \t g_shmem->allow_position: %d ",g_shmem->allow_brake,g_shmem->allow_position);

    // 调试输出（仅Debug模式有效）
    microkit_dbg_puts("Light scheduler initialized: position light allowed by default\n");
}

// ==============================================
// 5. 辅助函数：处理UART操作码（核心业务逻辑）
// ==============================================
static bool process_uart_command(uint8_t cmd) {
    bool need_notify = false;

    switch (cmd) {
        // ==========================================
        // 5.1 近光灯控制
        // ==========================================
        case UART_CMD_LOW_BEAM_ON:
            g_shmem->allow_low_beam = 1;
            need_notify = true;
            microkit_dbg_puts("Scheduler: UART cmd - Low beam ON allowed\n");
            break;

        case UART_CMD_LOW_BEAM_OFF:
            g_shmem->allow_low_beam = 0;
            // 互锁：近光灯关闭时禁止远光灯
            g_shmem->allow_high_beam = 0;
            need_notify = true;
            microkit_dbg_puts("Scheduler: UART cmd - Low beam OFF, high beam locked\n");
            break;

        // ==========================================
        // 5.2 远光灯控制
        // ==========================================
        case UART_CMD_HIGH_BEAM_ON:
            // 互锁检查：近光灯已开启
            if (g_shmem->allow_low_beam) {
                // 互锁检查：刹车灯未开启（预留，后续扩展刹车灯用）
                if (!g_shmem->allow_brake) {
                    g_shmem->allow_high_beam = 1;
                    need_notify = true;
                    microkit_dbg_puts("Scheduler: UART cmd - High beam ON allowed\n");
                } else {
                    microkit_dbg_puts("Scheduler: UART cmd - High beam ON denied (brake active)\n");
                }
            } else {
                microkit_dbg_puts("Scheduler: UART cmd - High beam ON denied (low beam off)\n");
            }
            break;

        case UART_CMD_HIGH_BEAM_OFF:
            g_shmem->allow_high_beam = 0;
            need_notify = true;
            microkit_dbg_puts("Scheduler: UART cmd - High beam OFF\n");
            break;

        // ==========================================
        // 5.3 左转向灯控制
        // ==========================================
        case UART_CMD_LEFT_TURN_ON:
            // 互锁检查：刹车灯未开启（预留）
            if (!g_shmem->allow_brake) {
                // 互锁检查：右转向灯未开启
                if (!g_shmem->allow_turn_right) {
                    g_shmem->allow_turn_left = 1;
                    need_notify = true;
                    microkit_dbg_puts("Scheduler: UART cmd - Left turn ON allowed\n");
                } else {
                    microkit_dbg_puts("Scheduler: UART cmd - Left turn ON denied (right turn active)\n");
                }
            } else {
                microkit_dbg_puts("Scheduler: UART cmd - Left turn ON denied (brake active)\n");
            }
            break;

        case UART_CMD_LEFT_TURN_OFF:
            g_shmem->allow_turn_left = 0;
            need_notify = true;
            microkit_dbg_puts("Scheduler: UART cmd - Left turn OFF\n");
            break;

        // ==========================================
        // 5.4 右转向灯控制
        // ==========================================
        case UART_CMD_RIGHT_TURN_ON:
            // 互锁检查：刹车灯未开启（预留）
            if (!g_shmem->allow_brake) {
                // 互锁检查：左转向灯未开启
                if (!g_shmem->allow_turn_left) {
                    g_shmem->allow_turn_right = 1;
                    need_notify = true;
                    microkit_dbg_puts("Scheduler: UART cmd - Right turn ON allowed\n");
                } else {
                    microkit_dbg_puts("Scheduler: UART cmd - Right turn ON denied (left turn active)\n");
                }
            } else {
                microkit_dbg_puts("Scheduler: UART cmd - Right turn ON denied (brake active)\n");
            }
            break;

        case UART_CMD_RIGHT_TURN_OFF:
            g_shmem->allow_turn_right = 0;
            need_notify = true;
            microkit_dbg_puts("Scheduler: UART cmd - Right turn OFF\n");
            break;

        // ==========================================
        // 5.5 无效操作码
        // ==========================================
        default:
            microkit_dbg_puts("Scheduler: UART cmd - Invalid command received\n");
            break;
    }

    return need_notify;
}

// ==============================================
// 6. 强制入口点2：收到通知时调用（单线程串行执行，天然原子）
// ==============================================
void notified(microkit_channel ch) {
    bool need_notify_light_control = false;

    // ==========================================
    // 6.1 处理UART命令模块的通知（唯一通道）
    // ==========================================
    if (ch == CH_UART_CMD) {
        // 从共享内存读取UART模块写入的操作码
        //uint8_t cmd = g_shmem->uart_cmd;
        uint8_t cmd = *(uint8_t*)input_buffer;
        // 处理操作码
        need_notify_light_control = process_uart_command(cmd);
        // 操作码处理完成后，重置为无效值，避免重复处理
        g_shmem->uart_cmd = 0xFF;
    }

    // ==========================================
    // 6.2 未知通道（仅Debug模式输出）
    // ==========================================
    else {
        microkit_dbg_puts("Scheduler: Unknown channel received\n");
    }

    // ==========================================
    // 6.3 若需要，通知灯光控制模块（异步非阻塞）
    // ==========================================
    if (need_notify_light_control) {
        microkit_notify(CH_LIGHT_CONTROL_ALLOW);
    }
}