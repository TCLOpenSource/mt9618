/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_COMMON_H
#define CXDREF_COMMON_H

#ifdef __linux__
#include <linux/types.h>
#else
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
#endif

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif

#if defined(_WINDOWS)
#include "cxdref_windows_sleep.h"
#define CXDREF_SLEEP(n) cxdref_windows_Sleep(n)
#elif defined(__linux__)
#include <linux/delay.h>
#define CXDREF_SLEEP(n) msleep(n)
#endif

#ifndef CXDREF_SLEEP
#error CXDREF_SLEEP(n) is not defined. This macro must be ported to your platform.
#endif

#define CXDREF_ARG_UNUSED(arg) ((void)(arg))

typedef enum {
    CXDREF_RESULT_OK,
    CXDREF_RESULT_ERROR_ARG,
    CXDREF_RESULT_ERROR_I2C,
    CXDREF_RESULT_ERROR_SW_STATE,
    CXDREF_RESULT_ERROR_HW_STATE,
    CXDREF_RESULT_ERROR_TIMEOUT,
    CXDREF_RESULT_ERROR_UNLOCK,
    CXDREF_RESULT_ERROR_RANGE,
    CXDREF_RESULT_ERROR_NOSUPPORT,
    CXDREF_RESULT_ERROR_CANCEL,
    CXDREF_RESULT_ERROR_OTHER,
    CXDREF_RESULT_ERROR_OVERFLOW,
    CXDREF_RESULT_OK_CONFIRM
} cxdref_result_t;

int32_t cxdref_Convert2SComplement(uint32_t value, uint32_t bitlen);

uint32_t cxdref_BitSplitFromByteArray(uint8_t *pArray, uint32_t startBit, uint32_t bitNum);

#define CXDREF_MACRO_MULTILINE_BEGIN  do {
#if ((defined _MSC_VER) && (_MSC_VER >= 1300))
#define CXDREF_MACRO_MULTILINE_END \
        __pragma(warning(push)) \
        __pragma(warning(disable:4127)) \
        } while(0) \
        __pragma(warning(pop))
#else
#define CXDREF_MACRO_MULTILINE_END } while(0)
#endif


#ifdef CXDREF_TRACE_ENABLE
void cxdref_trace_log_enter(const char* funcname, const char* filename, unsigned int linenum);
void cxdref_trace_log_return(cxdref_result_t result, const char* funcname, unsigned int linenum);
#define CXDREF_TRACE_ENTER(func) pr_info("[%s][%d]enter\n", __FUNCTION__, __LINE__)
#define CXDREF_TRACE_RETURN(result) \
    do { \
        cxdref_result_t result_tmp = (result); \
        pr_info("[%s][%d]result:%d\n", __FUNCTION__, __LINE__,result);\
        return result_tmp; \
        }while(0);
#else

#define CXDREF_TRACE_ENTER(func)

#define CXDREF_TRACE_RETURN(result) return(result)

#define CXDREF_TRACE_I2C_ENTER(func)

#define CXDREF_TRACE_I2C_RETURN(result) return(result)
#endif


#ifdef CXDREF_TRACE_I2C_ENABLE
void cxdref_trace_i2c_log_enter(const char* funcname, const char* filename, unsigned int linenum);
void cxdref_trace_i2c_log_return(cxdref_result_t result, const char* filename, unsigned int linenum);
#define CXDREF_TRACE_I2C_ENTER(func) cxdref_trace_i2c_log_enter((func), __FILE__, __LINE__)
#define CXDREF_TRACE_I2C_RETURN(result) \
    CXDREF_MACRO_MULTILINE_BEGIN \
        cxdref_result_t result_tmp = (result); \
        cxdref_trace_i2c_log_return(result_tmp, __FILE__, __LINE__); \
        return result_tmp; \
    CXDREF_MACRO_MULTILINE_END
#else
#define CXDREF_TRACE_I2C_ENTER(func)
#define CXDREF_TRACE_I2C_RETURN(result) return(result)
#endif


typedef struct cxdref_atomic_t {
    volatile int counter;
} cxdref_atomic_t;
#define cxdref_atomic_set(a,i) ((a)->counter = i)
#define cxdref_atomic_read(a) ((a)->counter)


typedef struct cxdref_stopwatch_t {
    uint32_t startTime;

} cxdref_stopwatch_t;

cxdref_result_t cxdref_stopwatch_start (cxdref_stopwatch_t * pStopwatch);

cxdref_result_t cxdref_stopwatch_sleep (cxdref_stopwatch_t * pStopwatch, uint32_t ms);

cxdref_result_t cxdref_stopwatch_elapsed (cxdref_stopwatch_t * pStopwatch, uint32_t* pElapsed);

#endif
