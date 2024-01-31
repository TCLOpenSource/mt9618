// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ":[%s:%d] " fmt, __func__, __LINE__

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/kallsyms.h>
#include <linux/scatterlist.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/tee_drv.h>
#include <uapi/linux/tee.h>

#include "mtktv-cpufreq.h"
/*********************************************************************
 *                             External                              *
 *********************************************************************/
extern int optee_version;

/*********************************************************************
 *                         Private Macro                             *
 *********************************************************************/

/*
 *1fc17090-bbdf-4648-bca9-3af97ad88e63
 *is generate from https://www.uuidgenerator.net/
 */
#define MTKTV_CPUFREQ_TA_UUID \
	{0x1fc17090, 0xbbdf, 0x4648,\
	{0xbc, 0xa9, 0x3a, 0xf9, 0x7a, 0xd8, 0x8e, 0x63} }

#define SEQUENCE_NUM            (8)

/* refernece tee.h */
#define TA_PARAM_NONE    (TEE_IOCTL_PARAM_ATTR_TYPE_NONE)
#define TA_PARAM_INPUT   (TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT)

/*********************************************************************
 *                         Private Structure                         *
 *********************************************************************/
struct uuid {
	u32 low;    // 4 bytes
	u16 middle; // 2 bytes
	u16 high;   // 2 bytes
	u8 sequence[SEQUENCE_NUM]; // 8 bytes
};

/*********************************************************************
 *                         Private Data                              *
 *********************************************************************/
static struct uuid cpufreq_uuid = MTKTV_CPUFREQ_TA_UUID;
static struct tee_context *ctx;
static int is_ta_file_open;
static u32 session;

/*********************************************************************
 *                         Private Function                          *
 *********************************************************************/
static int _optee_match(struct tee_ioctl_version_data *data, const void *vers)
{
	return 1;
}

static void _uuid_to_octets(uint8_t d[TEE_IOCTL_UUID_LEN], const struct uuid *s)
{
	d[0] = s->low >> 24;
	d[1] = s->low >> 16;
	d[2] = s->low >> 8;
	d[3] = s->low;
	d[4] = s->middle >> 8;
	d[5] = s->middle;
	d[6] = s->high >> 8;
	d[7] = s->high;
	memcpy(d + 8, s->sequence, sizeof(s->sequence));
}

static u32 _InitializeContext(void)
{
	if (optee_version == 3) {
		struct tee_ioctl_version_data vers = {
			.impl_id = TEE_OPTEE_CAP_TZ,
			.impl_caps = TEE_IMPL_ID_OPTEE,
			.gen_caps = TEE_GEN_CAP_GP,
		};

		ctx = tee_client_open_context(NULL,
				_optee_match, NULL, &vers);

		if (IS_ERR(ctx)) {
			pr_err("context is NULL\n");
			return TA_ERROR;
		}

		return TA_SUCCESS;
	}
	pr_err("only support optee version 3\n");
	return TA_ERROR;
}


static u32 _Open(void)
{
	if (optee_version == 3) {
		struct tee_ioctl_open_session_arg arg;
		struct tee_param param = {0};
		int rc;

		memset(&arg, 0, sizeof(arg));
		_uuid_to_octets(arg.uuid, &cpufreq_uuid);

		arg.clnt_login = TEE_IOCTL_LOGIN_PUBLIC;
		arg.num_params = 1;

		rc = tee_client_open_session(ctx, &arg,
			(struct tee_param *) &param);
		if ((rc == TA_SUCCESS) && (arg.ret == TA_SUCCESS)) {
			session = arg.session;
			is_ta_file_open = 1;
			return TA_SUCCESS;
		}

		pr_err("open session fail\n");
		return TA_ERROR;
	}
	pr_err("only support optee version 3\n");
	return TA_ERROR;
}

static u32 _InvokeCmd(u32 cmd_id, struct tee_param params)
{
	if (optee_version == 3) {
		struct tee_ioctl_invoke_arg arg;
		int rc;

		arg.num_params = 1;
		arg.session = session;
		arg.func = cmd_id;

		rc = tee_client_invoke_func(ctx, &arg, &params);
		if (rc) {
			pr_err("invoke cmd failed\n");
		}

		return TA_SUCCESS;
	}
	pr_err("only support optee version 3\n");
	return TA_ERROR;
}


/*********************************************************************
 *                          Public Function                          *
 *********************************************************************/

u32 MTKTV_CPUFREQ_TEEC_TA_Open(void)
{
	ta_result res;

	if (!ctx) {
		res = _InitializeContext();
		if (res != TA_SUCCESS) {
			pr_err("initial context fail\n");
			return TA_ERROR;
		}
	}

	if (!is_ta_file_open) {
		res = _Open();
		if (res != TA_SUCCESS) {
			pr_err("open session fail\n");
			return TA_ERROR;
		}
	}

	pr_info("Cpufreq TA Open Successfully\n");

	return TA_SUCCESS;
}
EXPORT_SYMBOL(MTKTV_CPUFREQ_TEEC_TA_Open);

u32 MTKTV_CPUFREQ_TEEC_Init(void)
{
	ta_result res;
	struct tee_param params;

	params.attr = TA_PARAM_NONE;
	params.u.value.a = 0;
	params.u.value.b = 0;

	res = _InvokeCmd(E_DVFS_OPTEE_INIT, params);

	if (res != TA_SUCCESS) {
		pr_err("invoke fail, cmd id: %d\n", E_DVFS_OPTEE_INIT);
		return TA_ERROR;
	}

	pr_info("Cpufreq TA Init Successfully\n");

	return TA_SUCCESS;
}
EXPORT_SYMBOL(MTKTV_CPUFREQ_TEEC_Init);

u32 MTKTV_CPUFREQ_TEEC_Adjust(u32 frequency)
{
	ta_result res;
	struct tee_param params;

	params.attr = TA_PARAM_INPUT;
	params.u.value.a = frequency;
	params.u.value.b = 0;

	res =  _InvokeCmd(E_DVFS_OPTEE_ADJUST, params);

	if (res != TA_SUCCESS) {
		pr_err("invoke fail, cmd id: %d\n", E_DVFS_OPTEE_ADJUST);
		return TA_ERROR;
	}

	pr_debug("Adjust Successfully, target: %d MHz\n", frequency);

	return TA_SUCCESS;

}
EXPORT_SYMBOL(MTKTV_CPUFREQ_TEEC_Adjust);

u32 MTKTV_CPUFREQ_TEEC_Suspend(void)
{
	ta_result res;
	struct tee_param params;

	params.attr = TA_PARAM_NONE;
	params.u.value.a = 0;
	params.u.value.b = 0;

	res = _InvokeCmd(E_DVFS_OPTEE_SUSPEND, params);
	if (res != TA_SUCCESS) {
		pr_err("invoke fail, cmd id: %d\n", E_DVFS_OPTEE_SUSPEND);
		return TA_ERROR;
	}

	pr_info("Suspend Successfully\n");

	return TA_SUCCESS;
}
EXPORT_SYMBOL(MTKTV_CPUFREQ_TEEC_Suspend);

u32 MTKTV_CPUFREQ_TEEC_Resume(void)
{
	ta_result res;
	struct tee_param params;

	params.attr = TA_PARAM_NONE;
	params.u.value.a = 0;
	params.u.value.b = 0;

	res = _InvokeCmd(E_DVFS_OPTEE_RESUME, params);
	if (res != TA_SUCCESS) {
		pr_err("invoke fail, cmd id: %d\n", E_DVFS_OPTEE_RESUME);
		return TA_ERROR;
	}

	pr_info("Resume Successfully\n");

	return TA_SUCCESS;
}
EXPORT_SYMBOL(MTKTV_CPUFREQ_TEEC_Resume);
MODULE_LICENSE("GPL");
