#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

struct Config {
    // 屏幕设备节点路径
    static const char* screenDevCluster() {
        return "/dev/screen_mcu/panel2-0/info";
    }
    
    static const char* screenDevIVI() {
        return "/dev/screen_mcu/panel0-0/info";
    }
    
    static const char* screenDevCeiling() {
        return "/dev/screen_mcu/panel2-1/info";
    }
    
    // I2C设备节点路径
    static const char* i2cDevCluster() {
        return "/dev/i2c7";
    }
    
    static const char* i2cDevIVI() {
        return "/dev/i2c2";
    }
    
    static const char* i2cDevCeiling() {
        return "/dev/i2c9";
    }
    
    // I2C地址 - 如果不取地址，这样定义没问题
    static const uint32_t I2C_ADDR_CLUSTER = 0x50;
    static const uint32_t I2C_ADDR_IVI = 0x51;
    static const uint32_t I2C_ADDR_CEILING = 0x52;
};

#endif
