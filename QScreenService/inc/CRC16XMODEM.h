#ifndef CRC16_XMODEM_H
#define CRC16_XMODEM_H

#include <cstdint>
#include <cstddef>
/**
 * @brief CRC16-XMODEM 算法实现
 * 
 * CRC16-CCITT (XMODEM)算法，多项式：0x1021，初始值：0x0000
 */
class CRC16XMODEM {
public:
    /**
     * @brief 计算数据的CRC16-XMODEM校验值
     * 
     * @param data 输入数据指针
     * @param length 数据长度
     * @return uint16_t CRC16校验值
     */
    static uint16_t calculate(const uint8_t* data, size_t length) {
        uint16_t crc = 0x0000;
        const uint16_t polynomial = 0x1021;
        
        for (size_t i = 0; i < length; i++) {
            crc ^= (static_cast<uint16_t>(data[i]) << 8);
            
            for (int j = 0; j < 8; j++) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ polynomial;
                } else {
                    crc <<= 1;
                }
            }
            crc &= 0xFFFF;
        }
        
        return crc;
    }
};
#endif // CRC16_XMODEM_H
