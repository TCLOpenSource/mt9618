/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_MATH_H_
#define CXDREF_MATH_H_

#include "cxdref_common.h"

uint32_t cxdref_math_log2 (uint32_t x);

uint32_t cxdref_math_log10 (uint32_t x);

uint32_t cxdref_math_log (uint32_t x);

#ifndef min
#   define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#endif
