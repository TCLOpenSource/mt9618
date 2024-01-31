/* SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause) */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _MTK_TV_DRM_VIDEO_OLED_H_
#define _MTK_TV_DRM_VIDEO_OLED_H_
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/component.h>
#include <linux/gpio/consumer.h>
#include "../mtk_tv_drm_drv.h"
#include "../mtk_tv_drm_crtc.h"

#define OLED_LUMIN_MAX  255

struct mtk_oled_status {
	bool    bErrDet;  // if ErrDet notify is sent, don't send it again
	bool    bComp;    // is compensation still running
	bool    bOffRs;   // is compensation running OFF-RS
	bool    bJb;      // is compensation running JB
	bool    bStill;   // is display image stay still
};

struct mtk_oled_gpio {
	int err_irq;
	int evdd_irq;
	int comp_irq;
	struct gpio_desc *gpio_err_det;
	struct gpio_desc *gpio_evdd_det;
	struct gpio_desc *gpio_on_rf;
	struct gpio_desc *gpio_comp_done;
	struct gpio_desc *gpio_qsm_en;
};

struct mtk_oled_i2c {
	uint8_t  slave;
	uint8_t  channel_id;
	uint8_t  i2c_mode;
	uint8_t  reg_offset;
	uint8_t  lumin_addr;
	uint8_t  temp_addr;
	uint8_t  offrs_addr;
	uint8_t  offrs_mask;
	uint8_t  jb_addr;
	uint8_t  jb_mask;
	uint8_t  hdr_addr;
	uint8_t  hdr_mask;
	uint8_t  lea_addr;
	uint8_t  lea_mask;
	uint8_t  tpc_addr;
	uint8_t  tpc_mask;
	uint8_t  cpc_addr;
	uint8_t  cpc_mask;
	uint8_t  opt_p0_addr;
	uint8_t  opt_start_addr;
	uint16_t opt_start_mask;
};

struct mtk_oled_sequence {
	uint16_t onrf_op;
	uint16_t onrf_delay;
	uint16_t offrs_op;
	uint16_t jb_op;
	uint16_t jb_on_off;
	uint16_t jb_temp_max;
	uint16_t jb_temp_min;
	uint16_t jb_cooldown;
	uint16_t qsm_on;
	uint16_t qsm_evdd_off;
	uint16_t qsm_off;
	uint16_t qsm_evdd_on;
	uint16_t mon_delay;
};

struct mtk_oled_info {
	int oled_pixelshift;
	bool oled_support;
	bool oled_ip_bypass_cust;
	struct mtk_oled_status oled_status;
	struct mtk_oled_gpio oled_gpio;
	struct mtk_oled_i2c oled_i2c;
	struct mtk_oled_sequence oled_seq;
};

// OLED TCON functions
int mtk_tv_drm_set_oled_callback_ioctl(
	struct drm_device *drm_dev,	void *data,	struct drm_file *file_priv);
void mtk_tv_drm_send_oled_callback(
	struct mtk_oled_info *oled_info,
	enum drm_mtk_oled_event eEvent);
int mtk_tv_drm_get_oled_temperature_ioctl(
	struct drm_device *drm_dev,	void *data,	struct drm_file *file_priv);
int mtk_tv_drm_set_oled_hdr_ioctl(
	struct drm_device *drm_dev,	void *data,	struct drm_file *file_priv);
int mtk_tv_drm_get_oled_event_ioctl(
	struct drm_device *drm_dev,	void *data,	struct drm_file *file_priv);
int mtk_oled_set_luminance(struct mtk_oled_info *oled_info, uint8_t u8Lumin);
int mtk_oled_set_offrs(void *data, bool bEnable);
int mtk_oled_set_jb(void *data, bool bEnable);

// OLED Init related functions
int mtk_oled_init(void *data, struct platform_device *pdev);



#endif
