#include "BinFileDecode.h"
#include <cstdint>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <arpa/inet.h>
BinFileDecode& BinFileDecode::getInstance() {
    static BinFileDecode instance;
    return instance;
}

uint32_t BinFileDecode::bytesToUint32(const uint8_t bytes[4]) {
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) |
           static_cast<uint32_t>(bytes[3]);
}

bool BinFileDecode::decode(const std::string &binFile) {
    std::ifstream file(binFile, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << binFile << std::endl;
        return false;
    }

    // 读取起始段
    file.read(reinterpret_cast<char*>(&header_.startSegment), 4);
    header_.startSegment = ntohl(header_.startSegment); 
    std::cout << "startSegment: 0x" << std::hex << std::setw(8) << std::setfill('0') << header_.startSegment << std::dec << std::endl;

    // 校验起始段是否为0xFEFEFEFE
    if (header_.startSegment != 0xFEFEFEFE) {
        std::cerr << "起始段不正确，不是有效的升级包文件" << std::endl;
        return false;
    }

    // 读取包头各字段(按格式顺序)
    file.read(reinterpret_cast<char*>(&header_.crcCheck), 4);
    header_.crcCheck = ntohl(header_.crcCheck);
    std::cout << "crcCheck: 0x" << std::hex << std::setw(8) << std::setfill('0') << header_.crcCheck << std::dec << std::endl;

    file.read(reinterpret_cast<char*>(&header_.headerLength), 4);
    header_.headerLength = ntohl(header_.headerLength);
    std::cout << "headerLength: 0x" << std::hex << std::setw(8) << std::setfill('0') << header_.headerLength << std::dec << std::endl;

    file.read(header_.productPN, 20); 
    std::cout << "productPN: 0x";
    for (int i = 0; i < 20; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                << static_cast<unsigned int>(static_cast<unsigned char>(header_.productPN[i]));
    }
    std::cout << std::dec << std::endl;

    file.read(reinterpret_cast<char*>(&header_.softwareVersion), 2);
    header_.softwareVersion = ntohs(header_.softwareVersion);
    std::cout << "softwareVersion: 0x" << std::hex << std::setw(4) << std::setfill('0') << header_.softwareVersion << std::dec << std::endl;

    file.read(reinterpret_cast<char*>(&header_.displayTouchVersion), 2);
    header_.displayTouchVersion = ntohs(header_.displayTouchVersion);
    std::cout << "displayTouchVersion: 0x" << std::hex << std::setw(4) << std::setfill('0') << header_.displayTouchVersion << std::dec << std::endl;

    file.read(reinterpret_cast<char*>(&header_.blockCount), 4);
    header_.blockCount = ntohl(header_.blockCount);
    std::cout << "blockCount: 0x" << std::hex << std::setw(8) << std::setfill('0') << header_.blockCount << std::dec << std::endl;

    // 读取4个Block标识区(每个12字节)
    for (int i = 0; i < 4; ++i) {
        file.read(header_.blockId[i], 12);
        std::cout << "blockId[" << i << "]:0x ";
        for (int j = 0; j < 12; ++j) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                    << static_cast<unsigned int>(static_cast<unsigned char>(header_.blockId[i][j]));
        }
        std::cout << std::dec << std::endl;
    }

    // 读取4个Block的起始地址和长度
    for (int i = 0; i < 4; ++i) {
        file.read(reinterpret_cast<char*>(&header_.blocks[i].startAddr), 4);
        //header_.blocks[i].startAddr = ntohl(header_.blocks[i].startAddr);
        file.read(reinterpret_cast<char*>(&header_.blocks[i].length), 4);
        //header_.blocks[i].length = ntohl(header_.blocks[i].length);
        std::cout << "block[" << i << "] startAddr: 0x";
        for (int j = 0; j < 4; ++j) {
            std::cout  << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<unsigned int>(header.blocks[i].startAddr[j]);
        }
        std::cout << std::dec << std::endl;
        
        // 打印length的每个字节
        std::cout << "block[" << i << "] length: 0x";
        for (int j = 0; j < 4; ++j) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<unsigned int>(header.blocks[i].length[j]);
        }
        std::cout << std::dec << std::endl;

        uint32_t startAddrNew = bytesToUint32(header.blocks[i].startAddr);
        uint32_t blockLengthNew = bytesToUint32(header.blocks[i].length);
        std::cout<<"byte To Int ------------------"<<std::endl;
        std::cout << "block[" << i << "] startAddr: 0x" << std::hex << std::setw(8) << std::setfill('0') << startAddrNew << std::dec << std::endl;  
        std::cout << "block[" << i << "] length: 0x" << std::hex << std::setw(8) << std::setfill('0') << blockLengthNew << std::dec << std::endl;
    }

    file.read(reinterpret_cast<char*>(&header_.wholePackageCrc), 4);
    header_.wholePackageCrc = ntohl(header_.wholePackageCrc);
    std::cout << "wholePackageCrc: 0x" << std::hex << std::setw(8) << std::setfill('0') << header_.wholePackageCrc << std::dec << std::endl;

    file.read(reinterpret_cast<char*>(&header_.endSegment), 4);
    header_.endSegment = ntohl(header_.endSegment);
    std::cout << "endSegment: 0x" << std::hex << std::setw(8) << std::setfill('0') << header_.endSegment << std::dec << std::endl;
    
    file.read(header_.reserved, 128);

    // 检查文件读取是否成功
    if (!file) {
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

static bool BinFileDecode::isBlockAllZero(const uint8_t block[12]) {
    static const uint8_t zeroBlock[12] = {0}; // 静态的零数组
    return memcmp(block, zeroBlock, 12) == 0;
}
