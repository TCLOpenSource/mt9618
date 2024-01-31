/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 */

#include <linux/i2c.h>
#include "cxd_dmd_i2c_device.h"

#define BURST_WRITE_MAX 128

static cxdref_result_t cxd_dmd_i2c_device_read (struct cxdref_i2c_t *p_i2c,
		u8 device_address, u8 *data, u32 size, u8 mode)
{
	struct cxd_dmd_i2c_device *i2c_device = NULL;

	struct i2c_msg msg[1] = {
		{
			.addr  = (device_address >>1),
			.flags = I2C_M_RD,
			.buf   = data,
			.len   = size
		}
	};

	if ((!p_i2c) || (!p_i2c->user) || (!data) || (size == 0))
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_ARG);

	i2c_device = (cxd_dmd_i2c_device*)(p_i2c->user);

	if (i2c_transfer(i2c_device->i2c, msg, 1) != 1)
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_I2C);

	CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t cxd_dmd_i2c_device_write (struct cxdref_i2c_t *p_i2c,
		u8 device_address, const u8 *data, u32 size, u8 mode)
{
	u8 buf[BURST_WRITE_MAX];
	struct cxd_dmd_i2c_device *i2c_device = NULL;

	struct i2c_msg msg[1] = {
		{
			.addr = (device_address >>1),
			.flags = 0,
			.buf = buf,
			.len = size
		},
	};

	if ((!p_i2c) || (!p_i2c->user) || (!data) || (size == 0) || (size > BURST_WRITE_MAX))
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_ARG);
    
	memcpy(&buf[0], data, size);

	i2c_device = (cxd_dmd_i2c_device*)(p_i2c->user);

	if (i2c_transfer(i2c_device->i2c, msg, 1) != 1)
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_I2C);

	CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t cxd_dmd_i2c_device_read_gw (struct cxdref_i2c_t *p_i2c,
		u8 device_address, u8 *data, u32 size, u8 mode)
{
	u8 buf[2];
	struct cxd_dmd_i2c_device *i2c_device = NULL;
    struct i2c_msg msg[2];

    if ((!p_i2c) || (!p_i2c->user) || (!data) || (size == 0))
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_ARG);
    
    msg[0].addr = (p_i2c->gwAddress >> 1);
    msg[0].flags = 0;
    msg[0].buf = buf;
    msg[0].len = 2;
    msg[1].addr = (p_i2c->gwAddress >> 1);
    msg[1].flags = I2C_M_RD;
    msg[1].buf = data;
    msg[1].len = size;
    
	buf[0] = p_i2c->gwSub;
	buf[1] = (device_address & 0xFE) | 0x01;

	i2c_device = (cxd_dmd_i2c_device*)(p_i2c->user);

	if (i2c_transfer(i2c_device->i2c, msg, 2) != 2)
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_I2C);

	CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t cxd_dmd_i2c_device_write_gw (struct cxdref_i2c_t *p_i2c,
		u8 device_address, const u8 *data, u32 size, u8 mode)
{
	u8 buf[BURST_WRITE_MAX + 2];
	struct cxd_dmd_i2c_device *i2c_device = NULL;
    struct i2c_msg msg;

    if ((!p_i2c) || (!p_i2c->user) || (!data) || (size == 0) || (size > BURST_WRITE_MAX))
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_ARG);
    
	msg.addr = (p_i2c->gwAddress >> 1);
	msg.flags = 0;
	msg.buf = buf;
	msg.len = size+2;

	buf[0] = p_i2c->gwSub;
	buf[1] = (device_address & 0xFE) | 0x00;
	memcpy(&buf[2], data, size);

	i2c_device = (cxd_dmd_i2c_device*)(p_i2c->user);

	if (i2c_transfer(i2c_device->i2c, &msg, 1) != 1)
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_I2C);

	CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t cxd_dmd_i2c_device_read_reg (struct cxdref_i2c_t *p_i2c,
		u8 device_address, u8 sub_address, u8 *data, u32 size)
{
	struct cxd_dmd_i2c_device *i2c_device = NULL;

	struct i2c_msg msg[2] = {
		{
			.addr = (device_address >> 1),
			.flags = 0,
			.buf = &sub_address,
			.len = 1
		}, {
			.addr = (device_address >> 1),
			.flags = I2C_M_RD,
			.buf = data,
			.len = size
		},
	};

	if ((!p_i2c) || (!p_i2c->user) || (!data) || (size == 0))
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_ARG);

	i2c_device = (cxd_dmd_i2c_device*)(p_i2c->user);

	if (i2c_transfer(i2c_device->i2c, msg, 2) != 2 )
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_I2C);

	CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_OK);
}

static cxdref_result_t cxd_dmd_i2c_device_write_reg_gw(struct cxdref_i2c_t *p_i2c,
		u8 device_address, u8 sub_address, const u8 *data, u32 size)
{
	u8 buf[BURST_WRITE_MAX + 3];
    struct cxd_dmd_i2c_device *i2c_device = NULL;
    struct i2c_msg msg;

    if ((!p_i2c) || (!p_i2c->user) || (!data) || (size == 0) || (size > BURST_WRITE_MAX))
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_ARG);
    
	msg.addr = (p_i2c->gwAddress >> 1);
	msg.flags = 0;
	msg.buf = buf;
	msg.len = size+3;

	buf[0] = p_i2c->gwSub;
	buf[1] = (device_address & 0xFE) | 0x00;
	buf[2] = sub_address;
	memcpy(&buf[3], data, size);

	i2c_device = (cxd_dmd_i2c_device*)(p_i2c->user);

	if (i2c_transfer(i2c_device->i2c, &msg, 1) != 1)
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_I2C);

	CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxd_dmd_i2c_device_initialize (struct cxd_dmd_i2c_device *i2c_device,
		struct i2c_adapter *i2c)
{
	if(!i2c_device || !i2c)
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_ARG);

	i2c_device->i2c = i2c;

	CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxd_dmd_i2c_device_finalize (struct cxd_dmd_i2c_device *i2c_device)
{
	if (!i2c_device)
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_ARG);

	i2c_device->i2c = NULL;

	CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxd_dmd_i2c_device_create_i2c (struct cxdref_i2c_t *p_i2c,
		struct cxd_dmd_i2c_device *i2c_device)
{
	if ((!p_i2c) || (!i2c_device))
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_ARG);

	p_i2c->Read = cxd_dmd_i2c_device_read;
	p_i2c->Write = cxd_dmd_i2c_device_write;
	p_i2c->ReadRegister = cxd_dmd_i2c_device_read_reg;
	p_i2c->WriteRegister = cxdref_i2c_CommonWriteRegister;
	p_i2c->WriteOneRegister = cxdref_i2c_CommonWriteOneRegister;
	p_i2c->gwAddress = 0;
	p_i2c->gwSub = 0;
	p_i2c->user = i2c_device;

	CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_OK);
}

cxdref_result_t cxd_dmd_i2c_device_create_i2c_gw (struct cxdref_i2c_t *p_i2c,
		struct cxd_dmd_i2c_device *i2c_device, u8 gw_address, u8 gw_sub)
{
	if ((!p_i2c) || (!i2c_device))
		CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_ERROR_ARG);

	p_i2c->Read = cxd_dmd_i2c_device_read_gw;
	p_i2c->Write = cxd_dmd_i2c_device_write_gw;
	p_i2c->ReadRegister = cxdref_i2c_CommonReadRegister;
	p_i2c->WriteRegister = cxd_dmd_i2c_device_write_reg_gw;
	p_i2c->WriteOneRegister = cxdref_i2c_CommonWriteOneRegister;
	p_i2c->gwAddress = gw_address;
	p_i2c->gwSub = gw_sub;
	p_i2c->user = i2c_device;

	CXDREF_TRACE_I2C_RETURN (CXDREF_RESULT_OK);
}
