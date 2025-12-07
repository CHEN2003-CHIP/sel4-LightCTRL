/**
 * @file log.h
 * @brief 车灯控制系统-日志打印组件头文件
 * @details 提供带文件、行号、函数名的结构化日志打印功能，支持自定义日志消息，
 *          适配seL4+Microkit环境的调试输出接口，符合华为C语言编码规范
 * @author USTC-CHEN
 * @date 2025-12-05
 * @note 依赖microkit.h和printf.h，日志输出基于microkit_dbg_puts/printf实现
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <microkit.h>
#include "printf.h"

/**
 * @brief 日志级别枚举（预留扩展）
 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,  /* 调试级日志，输出详细调试信息 */
    LOG_LEVEL_INFO  = 1,  /* 信息级日志，输出正常业务流程信息 */
    LOG_LEVEL_WARN  = 2,  /* 警告级日志，输出非致命异常信息 */
    LOG_LEVEL_ERROR = 3   /* 错误级日志，输出致命异常信息 */
} LogLevel;

/**
 * @brief 核心日志打印函数（内部使用）
 * @details 格式化日志内容，包含文件、行号、函数名和自定义消息
 * @param level 日志级别
 * @param file 打印日志的文件路径（建议使用__FILE__宏）
 * @param line 打印日志的行号（建议使用__LINE__宏）
 * @param func 打印日志的函数名（建议使用__func__宏）
 * @param fmt 自定义日志消息格式串（同printf格式）
 * @param ... 格式串对应的可变参数
 * @return 无
 */
static inline void log_print(LogLevel level, const char *file, int line, const char *func, const char *fmt, ...)
{
    /* 定义日志级别前缀 */
    const char *level_str = NULL;
    switch (level) {
        case LOG_LEVEL_DEBUG:
            level_str = "[DEBUG]";
            break;
        case LOG_LEVEL_INFO:
            level_str = "[INFO] ";
            break;
        case LOG_LEVEL_WARN:
            level_str = "[WARN] ";
            break;
        case LOG_LEVEL_ERROR:
            level_str = "[ERROR]";
            break;
        default:
            level_str = "[UNKNOWN]";
            break;
    }

    /* 拼接基础日志头（文件+行号+函数名） */
    char log_header[128] = {0};
    snprintf(log_header, sizeof(log_header), "%s [%s:%d] <%s> ", level_str, file, line, func);

    /* 处理可变参数，拼接自定义消息 */
    char log_msg[256] = {0};
    va_list args;
    va_start(args, fmt);
    vsnprintf(log_msg, sizeof(log_msg), fmt, args);
    va_end(args);

    /* 输出完整日志（适配Microkit调试输出接口） */
    microkit_dbg_puts(log_header);
    microkit_dbg_puts(log_msg);
    microkit_dbg_puts("\n");
}

/**
 * @brief 调试级日志宏（对外接口）
 * @details 自动填充文件、行号、函数名，打印调试级日志
 * @param fmt 自定义日志消息格式串
 * @param ... 格式串对应的可变参数
 */
#define LOG_DEBUG(fmt, ...) \
    log_print(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * @brief 信息级日志宏（对外接口）
 * @details 自动填充文件、行号、函数名，打印信息级日志
 * @param fmt 自定义日志消息格式串
 * @param ... 格式串对应的可变参数
 */
#define LOG_INFO(fmt, ...) \
    log_print(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * @brief 警告级日志宏（对外接口）
 * @details 自动填充文件、行号、函数名，打印警告级日志
 * @param fmt 自定义日志消息格式串
 * @param ... 格式串对应的可变参数
 */
#define LOG_WARN(fmt, ...) \
    log_print(LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * @brief 错误级日志宏（对外接口）
 * @details 自动填充文件、行号、函数名，打印错误级日志
 * @param fmt 自定义日志消息格式串
 * @param ... 格式串对应的可变参数
 */
#define LOG_ERROR(fmt, ...) \
    log_print(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#endif /* __LOG_H__ */