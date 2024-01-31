/*
 *  CXD Demodulator driver
 *
 *  Copyright 2022 Sony Semiconductor Solutions Corporation
 */

#ifndef CXDREF_I2C_H
#define CXDREF_I2C_H

#include "cxdref_common.h"

#define CXDREF_I2C_START_EN   (0x01)
#define CXDREF_I2C_STOP_EN    (0x02)

typedef struct cxdref_i2c_t {
    cxdref_result_t (*Read)(struct cxdref_i2c_t* pI2c, uint8_t deviceAddress, uint8_t* pData, uint32_t size, uint8_t mode);

    cxdref_result_t (*Write)(struct cxdref_i2c_t* pI2c, uint8_t deviceAddress, const uint8_t *pData, uint32_t size, uint8_t mode);
    
    cxdref_result_t (*ReadRegister)(struct cxdref_i2c_t* pI2c, uint8_t deviceAddress, uint8_t subAddress, uint8_t* pData, uint32_t size);
    
    cxdref_result_t (*WriteRegister)(struct cxdref_i2c_t* pI2c, uint8_t deviceAddress, uint8_t subAddress, const uint8_t *pData, uint32_t size);
    
    cxdref_result_t (*WriteOneRegister)(struct cxdref_i2c_t* pI2c, uint8_t deviceAddress, uint8_t subAddress, uint8_t data);

    uint8_t          gwAddress;
    uint8_t          gwSub;

    uint32_t         flags;
    void*            user;
} cxdref_i2c_t;

cxdref_result_t cxdref_i2c_CommonReadRegister(cxdref_i2c_t* pI2c, uint8_t deviceAddress, uint8_t subAddress, uint8_t* pData, uint32_t size);

cxdref_result_t cxdref_i2c_CommonWriteRegister(cxdref_i2c_t* pI2c, uint8_t deviceAddress, uint8_t subAddress, const uint8_t* pData, uint32_t size);

cxdref_result_t cxdref_i2c_CommonWriteOneRegister(cxdref_i2c_t* pI2c, uint8_t deviceAddress, uint8_t subAddress, uint8_t data);

cxdref_result_t cxdref_i2c_SetRegisterBits(cxdref_i2c_t* pI2c, uint8_t deviceAddress, uint8_t subAddress, uint8_t data, uint8_t mask);

#endif
