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
#include <sys/time.h>  // 添加这一行
#include <sel4/sel4.h>

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

// 近光灯引脚
#define PIN_LOW_BEAM    0   // 近光灯
#define PIN_HIGH_BEAM   1   // 远光灯
#define PIN_TURN_LEFT   2   // 左转向灯
#define PIN_TURN_RIGHT  3   // 右转向灯

// GPIO寄存器偏移量（根据硬件手册定义）
#define GPIO_DIR_OFFSET 0x00  // 方向寄存器偏移（假设）
#define GPIO_OUT_OFFSET 0x04  // 输出寄存器偏移

#define CMD_TARGET_MASK 0xF0  // 目标车灯掩码（高4位）
#define CMD_OP_MASK     0x0F  // 操作掩码（低4位）
#define CMD_TARGET(cmd) ((cmd >> 4) & 0x0F)  // 提取目标车灯（0-3）
#define CMD_OP(cmd)     (cmd & 0x0F)         // 提取操作（0=关，1=开）


/**
 * @brief 初始化定时器，设置闪烁频率（500ms一次中断）
 */
static void timer_init() {
    // 假设定时器时钟为1MHz，500ms对应加载值为500000
    *REG_PTR(timer_base_vaddr, TIMER_LOAD_OFFSET) = 5000000;
    
    // 配置控制寄存器：启动定时器，允许中断，自动重加载
    *REG_PTR(timer_base_vaddr, TIMER_CTRL_OFFSET) = (1 << 0) | (1 << 1) | (1 << 7);
}

//-----------------TEST--------------------------------------------------------------

// 方法2: 使用aarch64系统计数器（如果权限允许）
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
    *gpio_dir |= (1 << PIN_LOW_BEAM)   |  // 近光灯引脚设为输出
                 (1 << PIN_HIGH_BEAM)  |  // 远光灯引脚设为输出
                 (1 << PIN_TURN_LEFT)  |  // 左转向灯引脚设为输出
                 (1 << PIN_TURN_RIGHT);   // 右转向灯引脚设为输出

    LOG_INFO("GPIO PD初始化完成");
    LOG_INFO("  近光灯引脚(%d)、远光灯引脚(%d)、左转向灯引脚(%d)、右转向灯引脚(%d)均配置为输出",
           PIN_LOW_BEAM, PIN_HIGH_BEAM, PIN_TURN_LEFT, PIN_TURN_RIGHT);
    LOG_INFO("GPIO: starting\n");

    // 新增定时器初始化
    timer_init();
    LOG_INFO("GPIO PD初始化完成，定时器已启动（闪烁频率2Hz）");

    printf("%d\n",get_system_ticks());
    // LOG_INFO("时间: %ldms (500ms 开/关)",get_current_time_ms());
}

/**
 * @brief 定时器中断处理函数，切换转向灯状态
 */
static void timer_handle_irq() {
    // 清除定时器中断标志
    *REG_PTR(timer_base_vaddr, TIMER_INTCLR_OFFSET) = 1;

    // 左转向灯闪烁逻辑
    if (left_turn_active) {
        left_turn_state ^= 1;  // 翻转状态（0→1或1→0）
        volatile uint32_t* gpio_out = REG_PTR(gpio_base_vaddr, GPIO_OUT_OFFSET);
        if (left_turn_state) {
            *gpio_out |= (1 << PIN_TURN_LEFT);  // 点亮
            LOG_INFO("LEFT ON");
        } else {
            *gpio_out &= ~(1 << PIN_TURN_LEFT); // 熄灭
            LOG_INFO("LEFT_OFF");
        }
    }

    // 右转向灯闪烁逻辑
    if (right_turn_active) {
        right_turn_state ^= 1;  // 翻转状态
        volatile uint32_t* gpio_out = REG_PTR(gpio_base_vaddr, GPIO_OUT_OFFSET);
        if (right_turn_state) {
            *gpio_out |= (1 << PIN_TURN_RIGHT); // 点亮
        } else {
            *gpio_out &= ~(1 << PIN_TURN_RIGHT); // 熄灭
        }
    }

    // 确认中断处理完成
    microkit_irq_ack(TIMER_IRQ_CHANNEL);
}

/**
 * @brief 根据目标车灯编号获取对应GPIO引脚
 * @details 内部静态函数，映射车灯类型编号到物理引脚编号
 * @param target 目标车灯编号（0=近光，1=远光，2=左转向，3=右转向）
 * @return uint8_t 对应引脚编号；无效目标返回0xFF
 */
// 根据目标车灯获取对应引脚
static uint8_t get_pin_by_target(uint8_t target) {
    switch(target) {
        case 0: return PIN_LOW_BEAM;
        case 1: return PIN_HIGH_BEAM;
        case 2: return PIN_TURN_LEFT;
        case 3: return PIN_TURN_RIGHT;
        default: return 0xFF;  // 无效目标
    }
}


/**
 * @brief 根据目标车灯编号获取车灯名称（调试用）
 * @details 内部静态函数，映射车灯类型编号到中文名称，用于日志打印
 * @param target 目标车灯编号（0=近光，1=远光，2=左转向，3=右转向）
 * @return const char* 车灯名称字符串指针；无效目标返回"未知车灯"
 */
// 根据目标车灯获取名称[debug]
static const char* get_name_by_target(uint8_t target) {
    switch(target) {
        case 0: return "近光灯";
        case 1: return "远光灯";
        case 2: return "左转向灯";
        case 3: return "右转向灯";
        default: return "未知车灯";
    }
}

/**
 * @brief Microkit通道通知处理函数
 * @details 处理GPIO_CHANNEL通道的控制指令，解析指令后操作对应GPIO引脚，校验执行结果并上报状态/异常
 * @param channel 触发通知的通道编号
 * @return 无
 * @note 仅处理GPIO_CHANNEL通道，其他通道打印不支持提示；异常时通过FAULT_NOTIFY_CHANNEL上报故障
 */
void notified(microkit_channel channel) {
    if (channel == GPIO_CHANNEL) {
        // 从共享缓冲区读取命令（1字节：高4位目标，低4位操作）
        uint8_t cmd = *(uint8_t*)cmd_buffer;
        uint8_t target = CMD_TARGET(cmd);
        uint8_t op = CMD_OP(cmd);
        uint8_t pin = get_pin_by_target(target);

        if (pin == 0xFF) {
            LOG_ERROR("GPIO PD：无效命令（目标车灯=%d）", target);
            //告诉故障处理

            return;
        }

        // 操作GPIO输出寄存器
        uint32_t original_signal=-1;
        volatile uint32_t* gpio_out = REG_PTR(gpio_base_vaddr, GPIO_OUT_OFFSET);
        uint32_t original_gpio_val = *gpio_out;  // 读取操作前的完整GPIO_OUT值
        uint8_t original_state = (original_gpio_val >> pin) & 1;  // 提取目标引脚的原始状态（0=关，1=开）

        if(target!=2 && target!=3)
        {
            LOG_INFO("GPIO PD：%s原始状态（引脚=%d）：%s",
                     get_name_by_target(target), pin, original_state ? "开启" : "关闭");
            if (op == 1)
            {
                *gpio_out |= (1 << pin); // 开灯（置位对应引脚）
                LOG_INFO("GPIO PD：%s开启（引脚=%d）", get_name_by_target(target), pin);
            }
            else if (op == 0)
            {
                *gpio_out &= ~(1 << pin); // 关灯（清除对应引脚）
                LOG_INFO("GPIO PD：%s关闭（引脚=%d）", get_name_by_target(target), pin);
            }
            else
            {
                LOG_INFO("GPIO PD：无效操作（操作码=%d）", op);
            }

            // 校验相关寄存器是否真的修改成功
            //  检查执行结果（模拟故障检测：如命令执行后引脚状态未变则视为异常）
            //  操作后读取新状态
            uint32_t new_gpio_val = *gpio_out;             // 读取操作后的完整GPIO_OUT值
            uint8_t new_state = (new_gpio_val >> pin) & 1; // 提取目标引脚的新状态（0=关，1=开）
            LOG_INFO("GPIO PD：%s操作后状态（引脚=%d）：%s",
                     get_name_by_target(target), pin, new_state ? "开启" : "关闭");
            if (new_state != original_state)
            {
                // 通知lightctl成功
                microkit_notify(GPIO_CHANNEL);
            }
            else
            {
                // 失败通知faultmg
                microkit_notify(FAULT_NOTIFY_CHANNEL);
            }
        }
        //处理左转向
        else if(target==2){
            if (op == 1) {
                // 开启左转向灯：激活闪烁
                left_turn_active = true;
                LOG_INFO("左转向灯开始闪烁");
            } else {
                // 关闭左转向灯：停止闪烁并熄灭
                left_turn_active = false;
                left_turn_state = 0;
                volatile uint32_t* gpio_out = REG_PTR(gpio_base_vaddr, GPIO_OUT_OFFSET);
                *gpio_out &= ~(1 << PIN_TURN_LEFT);
                LOG_INFO("左转向灯关闭");
            }
        }
        // 处理右转向灯（target=3）
        else if (target == 3) {
            if (op == 1) {
                // 开启右转向灯：激活闪烁
                right_turn_active = true;
                LOG_INFO("右转向灯开始闪烁");
            } else {
                // 关闭右转向灯：停止闪烁并熄灭
                right_turn_active = false;
                right_turn_state = 0;
                volatile uint32_t* gpio_out = REG_PTR(gpio_base_vaddr, GPIO_OUT_OFFSET);
                *gpio_out &= ~(1 << PIN_TURN_RIGHT);
                LOG_INFO("右转向灯关闭");
            }
        }
        

    }
    // 新增定时器中断处理
    else if (channel == TIMER_IRQ_CHANNEL) {
        timer_handle_irq();
    }
    else {
        LOG_ERROR("其他请求无法响应\n");
    }
}