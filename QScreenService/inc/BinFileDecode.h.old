#ifndef BIN_FILE_DECODE_H
#define BIN_FILE_DECODE_H

#include <string>
#include <cstdint>
#include <cstring>

// 定义bin文件头结构体，对应所有解析字段
struct BinFileHeader {
    uint32_t startSegment;              // 起始段 4字节(0xFF,0xFF,0xFF,0xFF)
    uint32_t crcCheck;                  // 包头CRC校验段 4字节
    uint32_t headerLength;              // 包头长度 4字节(包含起始段到结束段的字节个数)
    uint8_t productPN[20];                 // 产品零件号 20字节(ASCII格式,未使用填充0x00)
    uint16_t softwareVersion;           // 软件版本号 2字节
    uint16_t displayTouchVersion;       // 显示屏与触摸版本号 2字节
    uint32_t blockCount;                // 升级包Block数量 4字节
    uint8_t blockId[4][12];                // Block1-4标识区 每个12字节
    struct BlockInfo {
        uint8_t startAddr[4];             // Block起始地址 4字节
        uint8_t length[4];                // Block长度 4字节(说明书,但是目前只有3个)
    } blocks[4];
    uint32_t wholePackageCrc;           // 升级包整包校验段 4字节
    uint32_t endSegment;                // 结束段 4字节(0xFF,0xFF,0xFF,0xFF)
    //char reserved[128];                 // 预留字节 128字节
};

class BinFileDecode {
public:
    static BinFileDecode& getInstance();
    bool decode(const std::string &binFile);  
    const BinFileHeader& getHeader() const { return header_; }
     bool isHeaderValid() const { return isHeaderValid_; }  //
     bool isBlockAllZero(const char block[12]);

private:
    BinFileHeader header_{};  // 存储解析后的文件头信息
    bool isHeaderValid_{false};  // 标记文件头是否有效
    static uint32_t bytesToUint32(const uint8_t bytes[4]); 
private:
    BinFileDecode() = default;
    ~BinFileDecode() = default;
    BinFileDecode(const BinFileDecode&) = delete;
    BinFileDecode& operator=(const BinFileDecode&) = delete;   
};

#endif // BIN_FILE_DECODE_H__
