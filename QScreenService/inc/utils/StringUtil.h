#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include <string>
#include <algorithm> 
#include <array>
class StringUtil {
public:
    // 格式化LCD命令：移除字符串中的所有空格
    static void formatLcdcommand(std::string& command) {
        command.erase(std::remove(command.begin(), command.end(), ' '), command.end());
    }
    std::array<uint8_t, 4> convertStartAddressToBytes(uint32_t startAddress) {
    std::array<uint8_t, 4> addressBytes{};
    // 按LSB到MSB的顺序拆分地址（小端字节序）
    addressBytes[0] = static_cast<uint8_t>(startAddress & 0xFF);           // Byte1: 最低有效位 (LSB)
    addressBytes[1] = static_cast<uint8_t>((startAddress >> 8) & 0xFF);   // Byte2: 次低有效位
    addressBytes[2] = static_cast<uint8_t>((startAddress >> 16) & 0xFF);  // Byte3: 次高有效位
    addressBytes[3] = static_cast<uint8_t>((startAddress >> 24) & 0xFF);  // Byte4: 最高有效位 (MSB)
    return addressBytes;
}

};

#endif // STRING_UTIL_H
