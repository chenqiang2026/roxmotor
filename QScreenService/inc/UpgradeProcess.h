/*******************************************************************************
 * Copyright (C) 2025
 * Company       Rox Motor Tech Co., Ltd.
 *               All Rights Reserved
 *               Secrecy Level STRICTLY CONFIDENTIAL
 * @file         UpgradeProcess.h
 * @author       qiang.chen01
 * @e-mail       qiang.chen01@ext.roxmotor.com
 * @brief        ScreenService project.
 *
 ******************************************************************************/
#ifndef UPGRADEPROCESS_H
#define UPGRADEPROCESS_H
#include <cstdint>
#include <mutex>
#include <thread>
#include <map> 
#include <unordered_map>
#include "BinFileDecode.h"
#include "SafeQueue.h"
#include "I2cProcessHandler.h"
#include <memory>

class UpgradeProcess {
public:
    UpgradeProcess();
    ~UpgradeProcess();
    int upgrade();
private:
    #pragma pack(push, 1)
    struct BinFileHeader {
        uint32_t startSegment;              // 起始段 4字节(0xFE,0xFE,0xFE,0xFE)
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
    #pragma pack(pop)  // 恢复默认内存对齐
private:
    int upgradeStep1();
    bool upgradeStep2();
    int upgradeStep3();
    int upgradeStep4();
    int upgradeStep5();
    int upgradeStep6();
    int ppsSubscribe();
    bool decodeBinFile(const std::string &binFile,std::string &binFilePN);
    int initScreenDevFd();
    int pollScreenDevFd();
    int readClusterFd(int fd,char *buf,int len);
    int readIviFd(int fd,char *buf,int len);
    int readCeilingFd(int fd,char *buf,int len);
    int lcdUpdatePBin(const  i2cDriverInfo &I2CInfo);
    int lcdUpdateTPBin(const  i2cDriverInfo &I2CInfo);
    int pollQueueLcdcommand();
    std::string getBinFileFromUpgradeQueue();
    void formatLcdcommand(std::string& command);
    std::array<uint8_t, 4> convertStartAddrToByteArray(uint32_t startAddress);

private:
    std::shared_ptr<BinFileDecode> binFileDecode_;
    int ppsFd_{-1};//和卢旭东 交互的 通信枢纽节点，订阅发布

    int curOpenScreenFd_{-1};//当前打开的屏幕fd
    std::thread lcdcommandReadThread_;
    std::thread pollQueueThread_;
    SafeQueue<ScreenCommand> safeQueue_{};
    std::mutex safeQueueMtx_{};
    std::condition_variable safeQueueCv_{};

    SafeQueue<ScreenCommand> upgradeCommandQueue_{};
    std::mutex upgradeMutex_{};
    std::condition_variable upgradeCv_{}; 

    std::map<int ,std::map<std::string,std::string>> lcdCommandMap_{};//缓存结果 不同屏幕 不同命令
    
};

#endif // UPGRADEPROCESS_H
