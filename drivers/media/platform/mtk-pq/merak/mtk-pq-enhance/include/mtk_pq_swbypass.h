/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MTK_PQ_SWBYPASS_H_
#define _MTK_PQ_SWBYPASS_H_

#ifdef _MTK_PQ_SWBYPASS_C_
#ifndef INTERFACE
#define INTERFACE
#endif
#else
#ifndef INTERFACE
#define INTERFACE	extern
#endif
#endif

//------------------------------------------------------------------------------
//  Macro and Define
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  Type and Structure
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  Variable
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  Function
//------------------------------------------------------------------------------
void mtk_pq_swbypass_cmd(struct device *dev, __u32 win_id, __u32 bypass, __u32 ip, __u32 show);
#undef INTERFACE
#endif		// _MTK_PQ_SWBYPASS_H_
