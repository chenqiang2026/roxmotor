/*===========================================================================*\
* Copyright 2025 Aptiv, Inc., All Rights Reserved.
* Aptiv Confidential
\*===========================================================================*/

#pragma once

#include <stdint.h>
#include <hw/i2c.h>
#include <mutex>
#include <string>
#include <optional>

struct i2cDriverInfo
{
    const std::string i2cDevPath;
    int i2cFd;
    uint32_t i2cDevAddr;
};

class i2cProcessHandler
{
private:
    static int i2c_open(const char* devname);
    static void i2c_close(int fd);
    static int i2c_set_slave_addr(int fd, uint32_t addr, uint32_t fmt);
    static int i2c_set_bus_speed(int fd, uint32_t speed, uint32_t *ospeed);
    static int i2c_bus_lock(int fd);
    static int i2c_bus_unlock(int fd);
    static int i2c_driver_info(int fd, i2c_driver_info_t *driverinfo);
    static int i2c_read(int fd, void *buf, uint32_t len);
    static int i2c_write(int fd, void *buf, uint32_t len);
    static int i2c_combined_writeread(int fd, void * wbuff, uint32_t wlen, void* rbuff, uint32_t rlen);
    static bool i2cBusLockHandler(int fd);
    static std::optional<std::reference_wrapper<std::mutex>> getI2cDeviceMutex(const std::string& dev);

    static std::mutex mtx_1;
    static std::mutex mtx_2;
    static const std::string i2cDevDispCSD;
    static const std::string i2cDevDispClusterAndRear;
    static constexpr uint8_t i2cRetryCount = 3;
public:
    static int i2cInitHandler(const std::string &dev);
    static bool i2cReadHandler(const i2cDriverInfo &devinfo,uint32_t reg,uint8_t reglen,uint8_t *val,uint32_t len);
    static bool i2cWriteHandler(const i2cDriverInfo &devinfo,uint8_t *val,uint32_t len);
    static void i2cDeinitHandler(int &fd);
};
