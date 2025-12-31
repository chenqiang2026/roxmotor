#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include <string>
#include <algorithm> 
#include <array>
#include <sstream>
#include <iomanip>
#include <vector>

class StringUtil {
public:
    // 格式化LCD命令：移除字符串中的所有空格
    static void formatLcdcommand(std::string& command) 
    {
        command.erase(std::remove(command.begin(), command.end(), ' '), command.end());
    }

    std::array<uint8_t, 4> convertStartAddressToBytes(uint32_t startAddress) 
    {
        std::array<uint8_t, 4> addressBytes{};
        // 按LSB到MSB的顺序拆分地址（小端字节序）
        addressBytes[0] = static_cast<uint8_t>(startAddress & 0xFF);           // Byte1: 最低有效位 (LSB)
        addressBytes[1] = static_cast<uint8_t>((startAddress >> 8) & 0xFF);   // Byte2: 次低有效位
        addressBytes[2] = static_cast<uint8_t>((startAddress >> 16) & 0xFF);  // Byte3: 次高有效位
        addressBytes[3] = static_cast<uint8_t>((startAddress >> 24) & 0xFF);  // Byte4: 最高有效位 (MSB)
        return addressBytes;
    }
    static std::string hexToSVer(const uint8_t* data, size_t len) 
    {
        std::vector<uint8_t> filteredData;
        // 过滤掉0x20（空格）字节
        for (size_t i = 0; i < len; i++) {
            if (data[i] != 0x20) {
                filteredData.push_back(data[i]);
            }
        }
        if (filteredData.size() < 3) {
            return "";
        }
        std::stringstream ss;
        // 第一个字节：直接使用，去掉前导0
        ss << std::hex << static_cast<int>(filteredData[0]);
        ss << "_";
        // 第二个字节：前面加上00，补足4位
        ss << std::setw(4) << std::setfill('0') 
        << static_cast<int>(filteredData[1]);
        ss << "_";  
        // 第三个字节：前面加上00，补足4位
        ss << std::setw(4) << std::setfill('0') 
        << static_cast<int>(filteredData[2]); 
        ss << "\n";      
        return ss.str();
    }
    static std::string hexToHVer(const uint8_t* data, size_t len) 
    {
        std::vector<uint8_t> filteredData;
        // 1. 过滤掉0x20（空格）字节
        for (size_t i = 0; i < len; i++) {
            if (data[i] != 0x20) {
                filteredData.push_back(data[i]);
            }
        }
        // 2. 确保有足够的非空格字节
        if (filteredData.size() < 4) {
            return "";
        }
        std::stringstream ss;
        // 3. 将前numBytes个字节转换为十进制，用点号分隔
        for (size_t i = 0; i < 4; i++) {
            ss << static_cast<int>(filteredData[i]);  // 转换为十进制整数
            if (i < 3) {
                ss << ".";
            }
        }  
        ss << "\n";      
        return ss.str();
    }

    static std::string hexToBlVer(const uint8_t* data, size_t len) 
    {
        if (len < 4) {
            return "";
        }
        std::stringstream ss;    
        for (size_t i = 0; i < 4; i++) {
            ss << static_cast<int>(data[i]); // 转换为十进制整数
            if (i < 3) {
                ss << ".";
            }
        }
        ss << "\n";      
        return ss.str();
    }
    static std::string hexToTpVer(const uint8_t* data, size_t len)
    {
        std::string result;
        for (size_t i = 0; i < len; i++) {
            // 0x20 是空格，可以选择保留或忽略
            // 如果需要保留空格，取消下面这行注释
            // result += static_cast<char>(data[i]);
            // 如果忽略空格，只添加非空格字符
            if (data[i] != 0x20) {
                result += static_cast<char>(data[i]);
            }
        }
        result += "\n";
        return result;
    }   

};

#endif // STRING_UTIL_H
