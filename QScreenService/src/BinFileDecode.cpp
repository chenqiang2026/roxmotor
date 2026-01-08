#include "../inc/BinFileDecode.h"
#include <cstdint>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <arpa/inet.h>
#include "../inc/Logger.h"

// BinFileDecode& BinFileDecode::getInstance() {
//     static BinFileDecode instance;
//     return instance;
// }
BinFileDecode::~BinFileDecode() 
{
    closeFile();
    // isHeaderValid_ = false;
    // isBlockMcuValid_ = false;
    // isBlockTconValid_ = false;
    // isBlockTouchValid_ = false;
}
uint32_t BinFileDecode::hexToDec(const uint8_t bytes[4]) {
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) |
           static_cast<uint32_t>(bytes[3]);
}

bool BinFileDecode::openFile(const std::string& filePath)
{
    closeFile();   
    binFilePath_ = filePath;
    binFileStream_.open(filePath, std::ios::binary | std::ios::in);
    if (!binFileStream_.is_open()) {
        SCREEN_LOGE("BinFileDecode::openFile无法打开文件: %s", filePath.c_str());
        std::cerr << "BinFileDecode::openFile无法打开文件: " << filePath << std::endl;
        return false;
    }
    
    isFileOpen_ = true;
    SCREEN_LOGD("BinFileDecode::openFile成功打开文件: %s", filePath.c_str());
    std::cout << "BinFileDecode::openFile成功打开文件: " << filePath << std::endl;
    return true;
}
    
void BinFileDecode::closeFile() 
{
    if (binFileStream_.is_open()) {
        binFileStream_.close();
        isFileOpen_ = false;
        SCREEN_LOGD("关闭文件: %s", binFilePath_.c_str());
    }
}

bool BinFileDecode::decodeBinFileHeadInfo() {
    if (!isFileOpen_) {
        SCREEN_LOGE("BinFileDecode::decodeBinFileHeadInfo文件未打开,无法解析头信息");
        std::cerr << "BinFileDecode::decodeBinFileHeadInfo文件未打开,无法解析头信息" << std::endl;
        return false;
    }
    header_ = BinFileHeader();
    isHeaderValid_ = false;
    isBlockMcuValid_ = false;
    isBlockTconValid_ = false;
    isBlockTouchValid_ = false;

    binFileStream_.seekg(0, std::ios::beg);
    if (!binFileStream_) {
        SCREEN_LOGE("无法定位到文件开头");
        return false;
    }
    // 读取起始段
    binFileStream_.read(reinterpret_cast<char*>(&header_.startSegment), 4);
    header_.startSegment = ntohl(header_.startSegment); 
    std::cout << "startSegment: 0x" << std::hex << std::setw(8) << std::setfill('0') << header_.startSegment << std::dec << std::endl;

    // 校验起始段是否为0xFEFEFEFE
    if (header_.startSegment != 0xFEFEFEFE) {
        std::cerr << "起始段不正确，不是有效的升级包文件" << std::endl;
        return false;
    }

    // 读取包头各字段(按格式顺序)
    binFileStream_.read(reinterpret_cast<char*>(&header_.crcCheck), 4);
    header_.crcCheck = ntohl(header_.crcCheck);
    std::cout << "crcCheck: 0x" << std::hex << std::setw(8) << std::setfill('0') << header_.crcCheck << std::dec << std::endl;

    binFileStream_.read(reinterpret_cast<char*>(&header_.headerLength), 4);
    header_.headerLength = ntohl(header_.headerLength);
    std::cout << "header_Length: 0x" << std::hex << std::setw(8) << std::setfill('0') << header_.headerLength << std::dec << std::endl;

    binFileStream_.read(header_.productPN, 20); 
    std::cout << "productPN:";
    for (int i = 0; i < 20; ++i) {
        std::cout << header_.productPN[i];
    }
    std::cout << std::endl;

    binFileStream_.read(reinterpret_cast<char*>(&header_.softwareVersion), 2);
    header_.softwareVersion = ntohs(header_.softwareVersion);
    std::cout << "softwareVersion: 0x" << std::hex << std::setw(4) << std::setfill('0') << header_.softwareVersion << std::dec << std::endl;

    binFileStream_.read(reinterpret_cast<char*>(&header_.displayTouchVersion), 2);
    header_.displayTouchVersion = ntohs(header_.displayTouchVersion);
    std::cout << "displayTouchVersion: 0x" << std::hex << std::setw(4) << std::setfill('0') << header_.displayTouchVersion << std::dec << std::endl;

    binFileStream_.read(reinterpret_cast<char*>(&header_.blockCount), 4);
    header_.blockCount = ntohl(header_.blockCount);   
    std::cout << "blockCount: " << header_.blockCount << std::endl;
    std::cout << "blockCount: 0x" << std::hex << std::setw(8) << std::setfill('0') << header_.blockCount << std::dec << std::endl;

    // 读取4个Block标识区(每个12字节)
    for (int i = 0; i < 4; ++i) {
        binFileStream_.read(reinterpret_cast<char*>(header_.blockId[i]), 12);
        std::cout << "blockId[" <<i  << "]:0x ";
        for (int j = 0; j < 12; ++j) {  
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                    << static_cast<unsigned int>(static_cast<unsigned char>(header_.blockId[i][j]));
        }   
        std::cout << std::endl;
    }

    if (header_.blockId[0][0] == 0x31) {
        isBlockMcuValid_ = true;
        header_.blocks[0].blockId = 0x31;
        header_.blocks[0].blockName = "MCU";
    } else if(header_.blockId[0][0] == 0x33) {
        isBlockTouchValid_ = true;
        header_.blocks[0].blockId = 0x33;
        header_.blocks[0].blockName = "TOUCH";
    }

    if (header_.blockId[1][0] == 0x32) {
        isBlockTconValid_ = true;
        header_.blocks[1].blockId = 0x32;
        header_.blocks[1].blockName = "TCON";
    }
   
    // 读取4个Block的起始地址和长度
    for (int i = 0; i < header_.blockCount; ++i) {
        binFileStream_.read(reinterpret_cast<char*>(header_.blocks[i].startAddr), 4);
        //header_.blocks[i].startAddr = ntohl(header_.blocks[i].startAddr);
        binFileStream_.read(reinterpret_cast<char*>(header_.blocks[i].length), 4);
        //header_.blocks[i].length = ntohl(header_.blocks[i].length);
        std::cout << "block[" << i << "] startAddr: 0x";
        for (int j = 0; j < 4; ++j) {
            std::cout  << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<unsigned int>(header_.blocks[i].startAddr[j]);
        }
        std::cout <<std::endl;
        
        // 打印length的每个字节
        std::cout << "block[" << i << "] length: 0x";
        for (int j = 0; j < 4; ++j) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<unsigned int>(header_.blocks[i].length[j]);
        }
        std::cout << std::endl;

        header_.blocks[i].startAddrDecValue = hexToDec(header_.blocks[i].startAddr);
        header_.blocks[i].lengthDecValue = hexToDec(header_.blocks[i].length);
        std::cout << "block[" << i << "] startAddrDecValue:" << std::dec<< header_.blocks[i].startAddrDecValue << std::endl;  
        std::cout << "block[" << i << "] lengthDecValue:" << std::dec<< header_.blocks[i].lengthDecValue << std::endl;
    }

    binFileStream_.read(reinterpret_cast<char*>(&header_.wholePackageCrc), 4);
    header_.wholePackageCrc = ntohl(header_.wholePackageCrc);
    std::cout << "wholePackageCrc: 0x" << std::hex << std::setw(8) << std::setfill('0') << header_.wholePackageCrc << std::dec << std::endl;

    binFileStream_.read(reinterpret_cast<char*>(&header_.endSegment), 4);
    header_.endSegment = ntohl(header_.endSegment);
    std::cout << "endSegment: 0x" << std::hex << std::setw(8) << std::setfill('0') << header_.endSegment << std::dec << std::endl;

    // 检查文件读取是否成功
    if (!binFileStream_) {
        std::cerr << "文件头解析失败，可能文件不完整" << std::endl;
        return false;
    }

    // TODO: 实现海微CRC计算方法并校验
    bool crcValid = true;  // 临时占位，需替换为实际CRC校验结果
    if (!crcValid) {
        std::cerr << "包头CRC校验失败" << std::endl;
        return false;
    } 
    isHeaderValid_ = true;
    return true;
}

 bool BinFileDecode::readBlockData(uint32_t offset, uint32_t length, std::vector<uint8_t>& data) 
 {
    if (!isFileOpen_) {
        SCREEN_LOGE("文件未打开，无法读取数据");
        return false;
    }
     
    binFileStream_.seekg(offset, std::ios::beg);
    if (!binFileStream_) {
        SCREEN_LOGE("定位到偏移 0x%08X 失败", offset);
        return false;
    }
      
    data.resize(length);
    // 读取数据
    binFileStream_.read(reinterpret_cast<char*>(data.data()), length);
    if (!binFileStream_) {
        SCREEN_LOGE("从偏移 0x%08X 读取 %u 字节失败", offset, length);
        data.clear();
        return false;
    }
    SCREEN_LOGD("从偏移 0x%08X 成功读取 %u 字节", offset, length);
    return true;
}
