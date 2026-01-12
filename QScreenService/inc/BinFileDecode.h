#ifndef BIN_FILE_DECODE_H
#define BIN_FILE_DECODE_H

#include <string>
#include <cstdint>
#include <cstring>
#include <vector>
#include <functional>
#include <fstream>

// 定义bin文件头结构体，对应所有解析字段
struct BinFileHeader {
    uint32_t startSegment;              // 起始段 4字节(0xFF,0xFF,0xFF,0xFF)
    uint32_t crcCheck;                  // 包头CRC校验段 4字节
    uint32_t headerLength;              // 包头长度 4字节(包含起始段到结束段的字节个数)
    char productPN[20];                 // 产品零件号 20字节(ASCII格式,未使用填充0x00)
    uint16_t softwareVersion;         // 软件版本号 2字节
    uint16_t displayTouchVersion;       // 显示屏与触摸版本号 2字节
    uint32_t blockCount;                // 升级包Block数量 4字节，目前是2个 MCU和TCON
    uint8_t blockId[4][12];                // Block1-4标识区 每个12字节,只有第一个字节有用，其他都是预留的。
    struct BlockInfo {
        uint8_t blockId;
        std::string blockName;             //MCU TCON TOUCH
        uint8_t startAddr[4];             // Block起始地址 4字节
        uint8_t length[4];
        uint32_t startAddrDecValue;        //转成十进制，方便计算
        uint32_t lengthDecValue;
    } blocks[4];
    uint32_t wholePackageCrc;           // 升级包整包校验段 4字节
    uint32_t endSegment;                // 结束段 4字节(0xFF,0xFF,0xFF,0xFF)
    //char reserved[128];                 // 预留字节 128字节

    BinFileHeader(): startSegment(0), crcCheck(0), headerLength(0),
                     softwareVersion(0), displayTouchVersion(0), 
                     blockCount(0), wholePackageCrc(0), endSegment(0) {
        memset(productPN, 0, sizeof(productPN));
        memset(blockId, 0, sizeof(blockId));
        for (int i = 0; i < 4; ++i) {
            blocks[i].blockId = 0;
            blocks[i].blockName = "";
            memset(blocks[i].startAddr, 0, sizeof(blocks[i].startAddr));
            memset(blocks[i].length, 0, sizeof(blocks[i].length));
            blocks[i].startAddrDecValue = 0;
            blocks[i].lengthDecValue = 0;
        }
    }
};

class BinFileDecode {
public:
    //static BinFileDecode& getInstance();
    BinFileDecode() = default;
    ~BinFileDecode();
    bool openFile(const std::string& filePath);
    void closeFile();
    bool decodeBinFileHeadInfo();   
    bool readBlockData(uint32_t offset, uint32_t length, std::vector<uint8_t>& data);
    const BinFileHeader& getHeader() const { return header_; }
    bool isHeaderValid() const { return isHeaderValid_; } 
public:
    bool getBlockMcuValid() const { return isBlockMcuValid_; }
    bool getBlockTconValid() const { return isBlockTconValid_; }
    bool getBlockTouchValid() const { return isBlockTouchValid_; }
private:
    BinFileHeader header_{};  // 存储解析后的文件头信息
    bool isHeaderValid_{false};  // 标记文件头是否有效
    bool isBlockMcuValid_{false}; //0x31-MCU, 
    bool isBlockTconValid_{false}; //0x32-TCON,
    bool isBlockTouchValid_{false}; //0x33-Touch
    std::string binFilePath_;         
    std::ifstream binFileStream_;             
    bool isFileOpen_{false};                   
  
    
private:
    uint32_t hexToDec(const uint8_t bytes[4]); 
   // BinFileDecode(const BinFileDecode&) = delete;
   // BinFileDecode& operator=(const BinFileDecode&) = delete;   
};

#endif // BIN_FILE_DECODE_H
