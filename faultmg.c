
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
#include "logger.h"

// 错误码定义
#define ERR_SPEED_LIMIT        0x01 // 车速超限
#define ERR_MODE_CONFLICT      0x02 // 模式冲突
#define ERR_INVALID_CMD        0x03 // 无效指令
#define ERR_HW_STATE_ERR       0x04 // 硬件状态不一致


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
uint32_t total_error_count = 0;

/**
 * @brief 根据错误码打印具体的错误信息
 */
static void print_error_details(uint8_t err_code) {
    switch(err_code) {
        case ERR_SPEED_LIMIT:
            LOG_WARN("FAULT_MGMT: 收到警告 - 车速超限拒绝操作");
            break;
        case ERR_MODE_CONFLICT:
            LOG_WARN("FAULT_MGMT: 收到警告 - 车灯模式互锁冲突");
            break;
        case ERR_INVALID_CMD:
            LOG_WARN("FAULT_MGMT: 收到警告 - 收到无效的通道或指令");
            break;
        case ERR_HW_STATE_ERR:
            LOG_WARN("FAULT_MGMT: 收到警告 - 硬件状态回读错误");
            break;
        default:
            LOG_WARN("FAULT_MGMT: 收到未知错误码: 0x%x", err_code);
            break;
    }
}

void init(void){
    LOG_INFO("FAULT_MGMT: 初始化完成，开始监测...\n");
}

/**
 * @brief Microkit通道通知处理函数
 * @details 监听通道通知事件，处理GPIO组件的错误上报，达到阈值时通知lightctl组件
 * @param channel 触发通知的通道编号
 * @return 无
 * @note 仅处理FAULTMG_GPIO通道的通知，其他通道打印不识别提示
 */
void notified(microkit_channel channel){
    if(channel == FAULTMG_LIGHTCTL){
        // 从 Message Register 0 中读取错误码 (对应 lightctl 里的 microkit_mr_set(0, ...))
        uint8_t error_code = (uint8_t) microkit_mr_get(0);
        
        total_error_count++;
        
        LOG_INFO("FAULT_MGMT: 收到异常通知 (总计: %d 次)", total_error_count);
        
        // 打印具体的错误详情
        print_error_details(error_code);

        // TODO:
        // 1. 累计3次严重错误后进入安全模式
        // 2. 点亮仪表盘的故障灯
        // 3. 通过其他通道通知其他模块
    }
    else{
        LOG_ERROR("FAULTMG: 该信号无法识别 (Channel: %d)\n", channel);
    }
}


