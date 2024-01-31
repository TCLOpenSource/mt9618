/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/* Copyright (c) 2020 MediaTek Inc.
 * Author Kevin Ho <kevin-yc.ho@mediatek.com>
 */

#ifndef _DT_BINDINGS_MAILBOX_MT5896_MBOX_CHAN_ID_MAPS_H
#define _DT_BINDINGS_MAILBOX_MT5896_MBOX_CHAN_ID_MAPS_H

/* For debug usage, do not use below IDs. */
#define MBOX_CHAN_ID_ORIGINATOR		0
#define MBOX_CHAN_ID_POSTMAN		127

/**
 * For module clients, please add your own ID here.
 * DO NOT use the same ID with others.
 * Please use below format as define string naming:
 *   MBOX_CHAN_ID_XXXXXXXX
 */
#define MBOX_CHAN_ID_VOICE	1
#define MBOX_CHAN_ID_FRC	54
#define MBOX_CHAN_ID_LED	57

#endif /* _DT_BINDINGS_MAILBOX_MT5896_MBOX_CHAN_ID_MAPS_H */
