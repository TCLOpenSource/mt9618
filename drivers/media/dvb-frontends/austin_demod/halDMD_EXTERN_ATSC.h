/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
 *
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _HALDMD_EXTERN_ATSC_H_
#define _HALDMD_EXTERN_ATSC_H_

// #include "drvDMD_ATSC.h"
#ifdef ADD_MERAK20_ATSC3
#include "../mtk-merak20-demod/demod_drv_atsc.h"
#else
#include "../mtk-merak-demod/demod_drv_atsc.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

extern MS_BOOL HAL_EXTERN_ATSC_IOCTL_CMD(DMD_ATSC_IOCTL_DATA *pData,
DMD_ATSC_HAL_COMMAND eCmd, void *pArgs);

#ifdef __cplusplus
}
#endif

#endif // _HALDMD_EXTERN_ATSC_H_
