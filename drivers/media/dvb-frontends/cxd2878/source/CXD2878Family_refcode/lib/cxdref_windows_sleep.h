/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_WINDOWS_SLEEP_H
#define CXDREF_WINDOWS_SLEEP_H

int cxdref_windows_Sleep (unsigned long sleepTimeMs);

int cxdref_windows_ImproveTimeResolution (void);

int cxdref_windows_RestoreTimeResolution (void);

#endif
