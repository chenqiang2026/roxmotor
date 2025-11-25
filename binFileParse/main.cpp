#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <arpa/inet.h>

using namespace std;
// 定义bin文件头结构体，对应所有解析字段
struct BinFileHeader {
    uint32_t startSegment;              // 起始段 4字节(0xFF,0xFF,0xFF,0xFF)
    uint32_t crcCheck;                  // 包头CRC校验段 4字节
    uint32_t headerLength;              // 包头长度 4字节(包含起始段到结束段的字节个数)
    char productPN[20];                 // 产品零件号 20字节(ASCII格式,未使用填充0x00)
    uint16_t softwareVersion;           // 软件版本号 2字节
    uint16_t displayTouchVersion;       // 显示屏与触摸版本号 2字节
    uint32_t blockCount;                // 升级包Block数量 4字节
    char blockId[4][12];                // Block1-4标识区 每个12字节
    struct BlockInfo {
        uint32_t startAddr;             // Block起始地址 4字节
        uint32_t length;                // Block长度 4字节
    } blocks[4];
    uint32_t wholePackageCrc;           // 升级包整包校验段 4字节
    uint32_t endSegment;                // 结束段 4字节(0xFF,0xFF,0xFF,0xFF)
    char reserved[128];                 // 预留字节 128字节
};
static bool decodeBinFile(const std::string &) ;

int main(int argc,char *argv[]){
    std::string binFile{"central_update.bin"};
    if(decodeBinFile(binFile)){
        std::cout << "decode binfile end"  << std::endl;
    }
    return 0;
}

static bool decodeBinFile(const std::string &binFile)
{
    std::ifstream file(binFile, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << binFile << std::endl;
        return false;
    }

    BinFileHeader header{};  // 初始化文件头结构体
     // 读取起始段
    file.read(reinterpret_cast<char*>(&header.startSegment), 4);
    header.startSegment = ntohl(header.startSegment); 
    std::cout << "startSegment: 0x" << std::hex << std::setw(8) << std::setfill('0') << header.startSegment << std::dec << std::endl;

    // 校验起始段是否为0xFEFEFEFE
    if (header.startSegment != 0xFEFEFEFE) {
        std::cerr << "起始段不正确，不是有效的升级包文件" << std::endl;
        return false;
    }

    // 读取包头各字段(按格式顺序)
    file.read(reinterpret_cast<char*>(&header.crcCheck), 4);
    header.crcCheck = ntohl(header.crcCheck);
    std::cout << "crcCheck: 0x" << std::hex << std::setw(8) << std::setfill('0') << header.crcCheck << std::dec << std::endl;

    file.read(reinterpret_cast<char*>(&header.headerLength), 4);
    header.headerLength = ntohl(header.headerLength);
    //std::cout << "headerLength: " << header.headerLength << std::endl;
    std::cout << "headerLength: 0x" << std::hex << std::setw(8) << std::setfill('0') << header.headerLength << std::dec << std::endl;

    file.read(header.productPN, 20); 
    //std::cout << "productPN: " << header.productPN << std::endl;
    //std::cout << "productPN: 0x" << std::hex << std::setw(40) << std::setfill('0') << header.productPN << std::dec << std::endl;
    
    std::cout << "productPN: 0x";
    for (int i = 0; i < 20; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                << static_cast<unsigned int>(static_cast<unsigned char>(header.productPN[i]));
    }
    std::cout << std::dec << std::endl;  // 恢复十进制输出

    file.read(reinterpret_cast<char*>(&header.softwareVersion), 2);
    header.softwareVersion = ntohs(header.softwareVersion);
    //std::cout << "softwareVersion: " << header.softwareVersion << std::endl;
    std::cout << "softwareVersion: 0x" << std::hex << std::setw(4) << std::setfill('0') << header.softwareVersion << std::dec << std::endl;

    file.read(reinterpret_cast<char*>(&header.displayTouchVersion), 2);
    header.displayTouchVersion = ntohs(header.displayTouchVersion);
    //std::cout << "displayTouchVersion: " << header.displayTouchVersion << std::endl;
    std::cout << "displayTouchVersion: 0x" << std::hex << std::setw(4) << std::setfill('0') << header.displayTouchVersion << std::dec << std::endl;

    file.read(reinterpret_cast<char*>(&header.blockCount), 4);
    header.blockCount = ntohl(header.blockCount);
    //std::cout << "blockCount: " << header.blockCount << std::endl;
    std::cout << "blockCount: 0x" << std::hex << std::setw(8) << std::setfill('0') << header.blockCount << std::dec << std::endl;

    // 读取4个Block标识区(每个12字节)
    for (int i = 0; i < 4; ++i) {
    file.read(header.blockId[i], 12);
    //std::cout << "blockId[" << i << "]: " << header.blockId[i] << std::endl;
    std::cout << "blockId[" << i << "]:0x ";
    for (int j = 0; j < 12; ++j) {  // 遍历12字节
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<unsigned int>(static_cast<unsigned char>(header.blockId[i][j]));
    }
    std::cout << std::dec << std::endl;  // 恢复十进制输出
}
     // 读取4个Block的起始地址和长度
    for (int i = 0; i < 4; ++i) {
        file.read(reinterpret_cast<char*>(&header.blocks[i].startAddr), 4);
        header.blocks[i].startAddr = ntohl(header.blocks[i].startAddr);
        file.read(reinterpret_cast<char*>(&header.blocks[i].length), 4);
        header.blocks[i].length = ntohl(header.blocks[i].length);
        std::cout << "block[" << i << "] startAddr: 0x" << std::hex << std::setw(8) << std::setfill('0') << header.blocks[i].startAddr << std::dec << std::endl;
        std::cout << "block[" << i << "] length: 0x" << std::hex << std::setw(8) << std::setfill('0') << header.blocks[i].length << std::dec << std::endl;
    }

    file.read(reinterpret_cast<char*>(&header.wholePackageCrc), 4);
    header.wholePackageCrc = ntohl(header.wholePackageCrc);
    std::cout << "wholePackageCrc: 0x" << std::hex << std::setw(8) << std::setfill('0') << header.wholePackageCrc << std::dec << std::endl;

    file.read(reinterpret_cast<char*>(&header.endSegment), 4);
    header.endSegment = ntohl(header.endSegment);
    std::cout << "endSegment: 0x" << std::hex << std::setw(8) << std::setfill('0') << header.endSegment << std::dec << std::endl;
    file.read(header.reserved, 128);

    // 检查文件读取是否成功
    if (!file) {
        std::cerr << "文件头解析失败，可能文件不完整" << std::endl;
        return false;
    }

    // TODO: 实现海微CRC计算方法并校验
    // 校验逻辑：从包头长度开始到升级包整包校验段计算CRC
    bool crcValid = true;  // 临时占位，需替换为实际CRC校验结果
    if (!crcValid) {
        std::cerr << "包头CRC校验失败" << std::endl;
        return false;
    }
    return true;
}
