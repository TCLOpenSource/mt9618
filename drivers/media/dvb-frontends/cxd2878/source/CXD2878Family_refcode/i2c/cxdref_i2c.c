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

#include "cxdref_i2c.h"

#include "cxdref_stdlib.h"

#define BURST_WRITE_MAX 128

cxdref_result_t cxdref_i2c_CommonReadRegister(cxdref_i2c_t* pI2c, uint8_t deviceAddress, uint8_t subAddress, uint8_t* pData, uint32_t size)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_I2C_ENTER("cxdref_i2c_CommonReadRegister");

    if(!pI2c || (!pData)){
        CXDREF_TRACE_I2C_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    result = pI2c->Write(pI2c, deviceAddress, &subAddress, 1, CXDREF_I2C_START_EN);
    if(result == CXDREF_RESULT_OK){
        result = pI2c->Read(pI2c, deviceAddress, pData, size, CXDREF_I2C_START_EN | CXDREF_I2C_STOP_EN);
    }

    CXDREF_TRACE_I2C_RETURN(result);
}

cxdref_result_t cxdref_i2c_CommonWriteRegister(cxdref_i2c_t* pI2c, uint8_t deviceAddress, uint8_t subAddress, const uint8_t* pData, uint32_t size)
{
    cxdref_result_t result = CXDREF_RESULT_OK;
    uint8_t buffer[BURST_WRITE_MAX + 1];

    CXDREF_TRACE_I2C_ENTER("cxdref_i2c_CommonWriteRegister");

    if(!pI2c || (!pData)){
        CXDREF_TRACE_I2C_RETURN(CXDREF_RESULT_ERROR_ARG);
    }
    if(size > BURST_WRITE_MAX){
        CXDREF_TRACE_I2C_RETURN(CXDREF_RESULT_ERROR_ARG);
    }

    buffer[0] = subAddress;
    cxdref_memcpy(&(buffer[1]), pData, size);

    result = pI2c->Write(pI2c, deviceAddress, buffer, size+1, CXDREF_I2C_START_EN | CXDREF_I2C_STOP_EN);
    CXDREF_TRACE_I2C_RETURN(result);
}

cxdref_result_t cxdref_i2c_CommonWriteOneRegister(cxdref_i2c_t* pI2c, uint8_t deviceAddress, uint8_t subAddress, uint8_t data)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_I2C_ENTER("cxdref_i2c_CommonWriteOneRegister");

    if(!pI2c){
        CXDREF_TRACE_I2C_RETURN(CXDREF_RESULT_ERROR_ARG);
    }
    result = pI2c->WriteRegister(pI2c, deviceAddress, subAddress, &data, 1);
    CXDREF_TRACE_I2C_RETURN(result);
}

cxdref_result_t cxdref_i2c_SetRegisterBits(cxdref_i2c_t* pI2c, uint8_t deviceAddress, uint8_t subAddress, uint8_t data, uint8_t mask)
{
    cxdref_result_t result = CXDREF_RESULT_OK;

    CXDREF_TRACE_I2C_ENTER("cxdref_i2c_SetRegisterBits");

    if(!pI2c){
        CXDREF_TRACE_I2C_RETURN(CXDREF_RESULT_ERROR_ARG);
    }
    if(mask == 0x00){
        CXDREF_TRACE_I2C_RETURN(CXDREF_RESULT_OK);
    }

    if(mask != 0xFF){
        uint8_t rdata = 0x00;
        result = pI2c->ReadRegister(pI2c, deviceAddress, subAddress, &rdata, 1);
        if(result != CXDREF_RESULT_OK){ CXDREF_TRACE_I2C_RETURN(result); }
        data = (uint8_t)((data & mask) | (rdata & (mask ^ 0xFF)));
    }

    result = pI2c->WriteOneRegister(pI2c, deviceAddress, subAddress, data);
    CXDREF_TRACE_I2C_RETURN(result);
}

