
/**
 * @file fault_mgmt.c
 * @brief 车灯控制系统-异常错误管理组件
 * @details 负责异常错误的监测与处理，累计GPIO组件上报的错误次数，达到阈值时通知lightctl组件执行告警、指示灯显示等操作
 * @note 基于seL4+Microkit框架实现，仅处理GPIO组件的错误上报事件
 * Author USTC-CHEN 2025-12-05
 */

#include <stdint.h>
#include <stdbool.h>
#include <microkit.h>
#include <stddef.h>
#include "printf.h"

/**
 * @def FAULTMG_GPIO
 * @brief 接收GPIO组件错误上报的通道号
 */
#define FAULTMG_GPIO 7
/**
 * @def FAULTMG_LIGHTCTL
 * @brief 通知lightctl组件处理异常的通道号
 */
#define FAULTMG_LIGHTCTL 5

/**
 * @var error_times
 * @brief 累计接收GPIO组件上报的错误次数
 * @note 错误次数达到3次时重置计数，并触发对lightctl的通知
 */
uint32_t error_times=0;

void init(void){
    microkit_dbg_puts("FAULT_MGMT: 初始化完成，开始监测...\n");
}

/**
 * @brief Microkit通道通知处理函数
 * @details 监听通道通知事件，处理GPIO组件的错误上报，达到阈值时通知lightctl组件
 * @param channel 触发通知的通道编号
 * @return 无
 * @note 仅处理FAULTMG_GPIO通道的通知，其他通道打印不识别提示
 */
void notified(microkit_channel channel){
    if(channel==FAULTMG_GPIO){
        error_times++;
        if(error_times>=3){
            error_times=0;
            microkit_notify(FAULTMG_LIGHTCTL);
        }
    }
    else{
        printf("FAULTMG:该信号无法识别\n");
    }
}


