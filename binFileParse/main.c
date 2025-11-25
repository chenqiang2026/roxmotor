#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>

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

static bool decodeBinFile(const char *binFile);

int main(int argc, char *argv[]) {
    const char *binFile = "../central_update.bin";
    if (decodeBinFile(binFile)) {
        printf("decode binfile end\n");
    }
    return 0;
}

static bool decodeBinFile(const char *binFile) {
    FILE *file = fopen(binFile, "rb");
    if (!file) {
        //fprintf(stderr, "无法打开文件: %s\n", binFile);
        fprintf(stderr, "无法打开文件: %s (当前目录: %s)\n", binFile, getcwd(NULL, 0));
        return false;
    }

    struct BinFileHeader header;
    memset(&header, 0, sizeof(struct BinFileHeader));  // 初始化结构体

    // 读取起始段
    if (fread(&header.startSegment, sizeof(header.startSegment), 1, file) != 1) {
        fprintf(stderr, "读取起始段失败\n");
        fclose(file);
        return false;
    }
    header.startSegment = ntohl(header.startSegment);
    printf("startSegment: 0x%08X\n", header.startSegment);

    // 校验起始段是否为0xFEFEFEFE
    if (header.startSegment != 0xFEFEFEFE) {
        fprintf(stderr, "起始段不正确，不是有效的升级包文件\n");
        fclose(file);
        return false;
    }

    // 读取包头各字段(按格式顺序)
    if (fread(&header.crcCheck, sizeof(header.crcCheck), 1, file) != 1) {
        fprintf(stderr, "读取crcCheck失败\n");
        fclose(file);
        return false;
    }
    header.crcCheck = ntohl(header.crcCheck);
    printf("crcCheck: 0x%08X\n", header.crcCheck);

    if (fread(&header.headerLength, sizeof(header.headerLength), 1, file) != 1) {
        fprintf(stderr, "读取headerLength失败\n");
        fclose(file);
        return false;
    }
    header.headerLength = ntohl(header.headerLength);
    printf("headerLength: 0x%08X\n", header.headerLength);

    if (fread(header.productPN, sizeof(header.productPN), 1, file) != 1) {
        fprintf(stderr, "读取productPN失败\n");
        fclose(file);
        return false;
    }
    printf("productPN: 0x");
    for (int i = 0; i < 20; i++) {
        printf("%02X", (unsigned char)header.productPN[i]);
    }
    printf("\n");

    if (fread(&header.softwareVersion, sizeof(header.softwareVersion), 1, file) != 1) {
        fprintf(stderr, "读取softwareVersion失败\n");
        fclose(file);
        return false;
    }
    header.softwareVersion = ntohs(header.softwareVersion);
    printf("softwareVersion: 0x%04X\n", header.softwareVersion);

    if (fread(&header.displayTouchVersion, sizeof(header.displayTouchVersion), 1, file) != 1) {
        fprintf(stderr, "读取displayTouchVersion失败\n");
        fclose(file);
        return false;
    }
    header.displayTouchVersion = ntohs(header.displayTouchVersion);
    printf("displayTouchVersion: 0x%04X\n", header.displayTouchVersion);

    if (fread(&header.blockCount, sizeof(header.blockCount), 1, file) != 1) {
        fprintf(stderr, "读取blockCount失败\n");
        fclose(file);
        return false;
    }
    header.blockCount = ntohl(header.blockCount);
    printf("blockCount: 0x%08X\n", header.blockCount);

    // 读取4个Block标识区(每个12字节)
    for (int i = 0; i < 4; ++i) {
        if (fread(header.blockId[i], sizeof(header.blockId[i]), 1, file) != 1) {
            fprintf(stderr, "读取blockId[%d]失败\n", i);
            fclose(file);
            return false;
        }
        printf("blockId[%d]:0x ", i);
        for (int j = 0; j < 12; j++) {
            printf("%02X", (unsigned char)header.blockId[i][j]);
        }
        printf("\n");
    }

    // 读取4个Block的起始地址和长度
    for (int i = 0; i < 4; ++i) {
        if (fread(&header.blocks[i].startAddr, sizeof(header.blocks[i].startAddr), 1, file) != 1) {
            fprintf(stderr, "读取block[%d] startAddr失败\n", i);
            fclose(file);
            return false;
        }
        if (fread(&header.blocks[i].length, sizeof(header.blocks[i].length), 1, file) != 1) {
            fprintf(stderr, "读取block[%d] length失败\n", i);
            fclose(file);
            return false;
        }
        header.blocks[i].startAddr = ntohl(header.blocks[i].startAddr);
        header.blocks[i].length = ntohl(header.blocks[i].length);
        printf("block[%d] startAddr: 0x%08X\n", i, header.blocks[i].startAddr);
        printf("block[%d] length: 0x%08X\n", i, header.blocks[i].length);
    }

    if (fread(&header.wholePackageCrc, sizeof(header.wholePackageCrc), 1, file) != 1) {
        fprintf(stderr, "读取wholePackageCrc失败\n");
        fclose(file);
        return false;
    }
    header.wholePackageCrc = ntohl(header.wholePackageCrc);
    printf("wholePackageCrc: 0x%08X\n", header.wholePackageCrc);

    if (fread(&header.endSegment, sizeof(header.endSegment), 1, file) != 1) {
        fprintf(stderr, "读取endSegment失败\n");
        fclose(file);
        return false;
    }
    printf("endSegment: 0x%08X\n", header.endSegment);

    if (fread(header.reserved, sizeof(header.reserved), 1, file) != 1) {
        fprintf(stderr, "读取reserved失败\n");
        fclose(file);
        return false;
    }

    fclose(file);

    // 检查文件读取是否成功
    // TODO: 实现海微CRC计算方法并校验
    // 校验逻辑：从包头长度开始到升级包整包校验段计算CRC
    bool crcValid = true;  // 临时占位，需替换为实际CRC校验结果
    if (!crcValid) {
        fprintf(stderr, "包头CRC校验失败\n");
        return false;
    }
    return true;
}
