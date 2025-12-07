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

uintptr_t cmd_buffer;
uintptr_t input_buffer;  // 由系统描述文件的setvar_vaddr自动赋值

void init(void) {
    LOG_INFO("LIGHTCTL : starting");
}

/**
 * @brief 接收并转发车灯控制指令
 * @details 从input_buffer读取commandin组件下发的控制指令，打印指令日志后写入cmd_buffer，
 *          并通知GPIO组件处理该指令
 * @param 无
 * @return 无
 * @note 指令为1字节格式（高4位=目标车灯，低4位=操作类型），直接透传至GPIO组件
 */
void recieve_command()
{
    uint8_t cmd = *(uint8_t*)input_buffer;
    LOG_INFO("lightctl:收到信号码：%x",cmd);
    LOG_INFO("转述进gpio 通信通道");

    //SEND THE CMD
    char* cmdbuf=(char*)cmd_buffer;
    *(uint8_t*)cmdbuf=cmd;
    microkit_notify(LIGHTCTL_CHANNEL);
    
}

/**
 * @brief Microkit通道通知处理函数
 * @details 处理不同通道的通知事件，分别响应GPIO操作结果、commandin指令、故障管理异常通知
 * @param channel 触发通知的通道编号
 * @return 无
 * @note 仅处理预定义的三个通道，其他通道无响应动作
 */
void notified(microkit_channel channel) {
    
    switch(channel)
    {
        case LIGHTCTL_CHANNEL:
            LOG_INFO("LIGHTCTL:RECIEVE GPIO SUCCESS!");
            break;
        case LIGHTCTL_COMMANDIN_CHANNEL:
            recieve_command();
            break;
        case LIGHTCTL_FAULTMG_CHANNEL:
            LOG_INFO("LIGHTCTL:FAILED TO CHANGE THE LIGHT | ERROR!");
            break;
        default:
            break;
    }
}
