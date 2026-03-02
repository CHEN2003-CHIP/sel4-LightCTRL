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
//示廓灯、刹车灯
#define UART_CMD_POSITION_OFF   0x40
#define UART_CMD_POSITION_ON    0x41
#define UART_CMD_BRAKE_OFF      0x50
#define UART_CMD_BRAKE_ON       0x51


// ==============================================
// 共享内存数据结构 
// ==============================================
// 位掩码定义 (Bit Masks)
#define FLAG_ALLOW_BRAKE       (1UL << 0)
#define FLAG_ALLOW_TURN_LEFT   (1UL << 1)
#define FLAG_ALLOW_TURN_RIGHT  (1UL << 2)
#define FLAG_ALLOW_LOW_BEAM    (1UL << 3)
#define FLAG_ALLOW_HIGH_BEAM   (1UL << 4)
#define FLAG_ALLOW_POSITION    (1UL << 5)

// 辅助宏：检查位是否置位
#define IS_FLAG_SET(flags, mask)  (((flags) & (mask)) != 0)

// ==============================================
// 共享内存数据结构（仅调度器可写状态位，UART模块可写cmd）
// ==============================================
typedef struct {
    // UART命令模块写入的操作码
    volatile uint8_t  uart_cmd;

    // 【核心修改】使用单一变量管理所有允许位
    volatile uint32_t allow_flags;

    // 原始信号参数
    volatile uint8_t  turn_switch_pos;
    volatile uint8_t  beam_switch_pos;
    volatile uint16_t vehicle_speed;
} light_shmem_t;


// 共享内存指针
// static light_shmem_t *const g_shmem = (light_shmem_t *const)shared_memory_base_vaddr;
static light_shmem_t * g_shmem = NULL;
// ==============================================
// 4. 强制入口点1：系统启动时调用（仅初始化一次）
// ==============================================
void init(void) {
    g_shmem = (light_shmem_t *)shared_memory_base_vaddr;
    g_shmem->uart_cmd = 0xFF;

    g_shmem->allow_flags = 0; // 先全部清零
    g_shmem->allow_flags |= FLAG_ALLOW_POSITION; // 置位示廓灯

    g_shmem->turn_switch_pos = 0;
    g_shmem->beam_switch_pos = 0;
    g_shmem->vehicle_speed    = 10;

    LOG_INFO("g_shmem->allow_brake:%d \t g_shmem->allow_position: %d ",
             IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_BRAKE),
             IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_POSITION));

    LOG_INFO("Light scheduler initialized: position light allowed by default\n");
}

// ==============================================
// 5. 辅助函数：处理UART操作码
// ==============================================
static bool process_uart_command(uint8_t cmd) {
    bool need_notify = false;

    switch (cmd) {
        // ==========================================
        // 近光灯控制
        // ==========================================
        case UART_CMD_LOW_BEAM_ON:
            g_shmem->allow_flags |= FLAG_ALLOW_LOW_BEAM; // 置位
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Low beam ON allowed\n");
            break;

        case UART_CMD_LOW_BEAM_OFF:
            g_shmem->allow_flags &= ~FLAG_ALLOW_LOW_BEAM; // 清零
            // 互锁：近光灯关闭时禁止远光灯
            g_shmem->allow_flags &= ~FLAG_ALLOW_HIGH_BEAM;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Low beam OFF, high beam locked\n");
            break;

        // ==========================================
        // 远光灯控制
        // ==========================================
        case UART_CMD_HIGH_BEAM_ON:
            // 互锁检查：近光灯已开启
            if (IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_LOW_BEAM)) {
                // 互锁检查：刹车灯未开启
                if (!IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_BRAKE)) {
                    g_shmem->allow_flags |= FLAG_ALLOW_HIGH_BEAM;
                    need_notify = true;
                    LOG_INFO("Scheduler: UART cmd - High beam ON allowed\n");
                } else {
                    LOG_INFO("Scheduler: UART cmd - High beam ON denied (brake active)\n");
                }
            } else {
                LOG_INFO("Scheduler: UART cmd - High beam ON denied (low beam off)\n");
            }
            break;

        case UART_CMD_HIGH_BEAM_OFF:
            g_shmem->allow_flags &= ~FLAG_ALLOW_HIGH_BEAM;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - High beam OFF\n");
            break;

        // ==========================================
        // 左转向灯控制
        // ==========================================
        case UART_CMD_LEFT_TURN_ON:
            if (!IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_BRAKE)) {
                if (!IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_TURN_RIGHT)) {
                    LOG_INFO("Pre-check: allow_turn_left=%d", IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_TURN_LEFT));
                    g_shmem->allow_flags |= FLAG_ALLOW_TURN_LEFT;
                    LOG_INFO("Post-check: allow_turn_left=%d", IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_TURN_LEFT));
                    need_notify = true;
                    LOG_INFO("Scheduler: UART cmd - Left turn ON allowed\n");
                } else {
                    LOG_INFO("Scheduler: UART cmd - Left turn ON denied (right turn active)\n");
                }
            } else {
                LOG_INFO("Scheduler: UART cmd - Left turn ON denied (brake active)\n");
            }
            break;

        case UART_CMD_LEFT_TURN_OFF:
            g_shmem->allow_flags &= ~FLAG_ALLOW_TURN_LEFT;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Left turn OFF\n");
            break;

        // ==========================================
        //右转向灯控制
        // ==========================================
        case UART_CMD_RIGHT_TURN_ON:
            if (!IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_BRAKE)) {
                if (!IS_FLAG_SET(g_shmem->allow_flags, FLAG_ALLOW_TURN_LEFT)) {
                    g_shmem->allow_flags |= FLAG_ALLOW_TURN_RIGHT;
                    need_notify = true;
                    LOG_INFO("Scheduler: UART cmd - Right turn ON allowed\n");
                } else {
                    LOG_INFO("Scheduler: UART cmd - Right turn ON denied (left turn active)\n");
                }
            } else {
                LOG_INFO("Scheduler: UART cmd - Right turn ON denied (brake active)\n");
            }
            break;

        case UART_CMD_RIGHT_TURN_OFF:
            g_shmem->allow_flags &= ~FLAG_ALLOW_TURN_RIGHT;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Right turn OFF\n");
            break;
        
        // ==========================================
        // 示廓灯控制
        // ==========================================
        case UART_CMD_POSITION_ON:
            g_shmem->allow_flags |= FLAG_ALLOW_POSITION;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Position light ON allowed\n");
            break;

        case UART_CMD_POSITION_OFF:
            g_shmem->allow_flags &= ~FLAG_ALLOW_POSITION;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Position light OFF\n");
            break;

        // ==========================================
        // 刹车灯控制
        // ==========================================
        case UART_CMD_BRAKE_ON:
            g_shmem->allow_flags |= FLAG_ALLOW_BRAKE;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Brake ON allowed\n");
            break;

        case UART_CMD_BRAKE_OFF:
            g_shmem->allow_flags &= ~FLAG_ALLOW_BRAKE;
            need_notify = true;
            LOG_INFO("Scheduler: UART cmd - Brake OFF\n");
            break;

        default:
            LOG_INFO("Scheduler: UART cmd - Invalid command received\n");
            break;
    }

    return need_notify;
}



// ==============================================
// 收到通知时调用（单线程串行执行，天然原子）
// ==============================================
void notified(microkit_channel ch) {
    bool need_notify_light_control = false;

    if (ch == CH_UART_CMD) {
        uint8_t cmd = *(uint8_t*)input_buffer;
        need_notify_light_control = process_uart_command(cmd);
        g_shmem->uart_cmd = 0xFF;
    } else {
        LOG_INFO("Scheduler: Unknown channel received\n");
    }

    if (need_notify_light_control) {
        microkit_notify(CH_LIGHT_CONTROL_ALLOW);
    }
}