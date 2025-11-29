#include "UpgradeProcess.h"
#include <cstdint>
#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h> 
#include <sys/poll.h>
#include <unistd.h> 
#include <cstring>
#include "CRC16XMODEM.h"

using namespace std;
struct i2cDriverInfo;

constexpr const uint8_t bufSize = 128;
constexpr const uint8_t pollTimeout = 100;
constexpr const uint8_t pN = 20;
constexpr const uint8_t binPNIndex = 12;
constexpr const uint8_t pNDataStart = 3;
constexpr const uint8_t pNDataLength = 20;
constexpr const uint8_t devPollFdNum = 3;

constexpr const uint32_t mcuStartAddr = 0x00000000;
constexpr const uint32_t touchStartAddr = 0x00008000;
constexpr const uint32_t tconStartAddr = 0x00010000;

UpgradeProcess::UpgradeProcess()
{
   // queueThread_ = std::thread(&UpgradeProcess::pollQueueLcdcommand, this);

}

UpgradeProcess::~UpgradeProcess()
{
    // if(subscribeThread_.joinable()){
    //     subscribeThread_.join();
    // }
}

int UpgradeProcess::upgrade()
{    
    const std::string i2cDevice = iviI2cDevPath;
    int i2cFd = -1;
    uint32_t i2cAddr = iviI2cAddr;

    i2cDriverInfo I2CInfo = {
        .i2cDevPath = i2cDevice, 
        .i2cFd = i2cFd, 
        .i2cDevAddr = i2cAddr
    };
     
    I2CInfo.i2cFd = i2cProcessHandler::i2cInitHandler(I2CInfo.i2cDevPath);
    if (-1 == I2CInfo.i2cFd)
    { 
        LOG_D("%s %si2cInit failed", LOG_TAG,__func__);
        std::cout<<"i2cInit failed"<<std::endl;
        return -1;
    }
    //lcdGetSver(I2CInfo);
    //lcdGetHver(I2CInfo);
    //lcdGetBlver(I2CInfo);
    //lcdGetTpver(I2CInfo);
    //lcdUpdatePBin(I2CInfo);
    //lcdUpdateTPBin(I2CInfo);
    //initDevFd();

    //step1
    //主机通过(0x40 0x22 0xFEF3)通知读取显示屏的I2C通信协议版本
    upgradeStep1();
    //step2
    //查询display的PN
    bool isStep2UpgradeSuccess = upgradeStep2();
    if(!isStep2UpgradeSuccess){
        return -1;
    }
    upgradeStep3();

    //deinit
    i2cProcessHandler::i2cDeinitHandler(I2CInfo.i2cFd);

}
//Transfer Data 
int UpgradeProcess::upgradeStep6()
{
    
    
}
//Request Download 
int UpgradeProcess::upgradeStep5()
{
    const uint8_t* mcuBlockByteLength = binFileDecode_->getHeader().blocks[0].length; 
    uint8_t requestDownloadReqParamTemp[] = {0x60,0x82,0x00,0x00,0x00,0x00,mcuBlockByteLength[3],mcuBlockByteLength[2],mcuBlockByteLength[1],mcuBlockByteLength[0]};
    uint8_t requestDownloadRespData[4]={0x0};
    uint8_t crcBytes[2];
    uint8_t requestDownloadRespDate[4]={0x0};
    uint16_t crc = CRC16XMODEM::calculate(requestDownloadReqParamTemp + 1, sizeof(eraseRangeReqParam) - 1);
    uint8_t requestDownloadReqParam[] = {0x60,0x82,0x00,0x00,0x00,0x00,mcuBlockByteLength[3],mcuBlockByteLength[2],mcuBlockByteLength[1],mcuBlockByteLength[0],crcBytes[0],crcBytes[1]};
    crcBytes[0] = crc & 0xFF;
    crcBytes[1] = (crc >> 8) & 0xFF;
    retry_write:
    if(i2cProcessHandler::i2cWriteHandler(I2CInfo,requestDownloadReqParam,sizeof(requestDownloadReqParam)/sizeof(uint8_t))) {
        LOG_D("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",I2CInfo.i2cDevAddr);
    } else {
        LOG_E("主机通过 [0x60 0x60] 通知 display 开始下载失败\n");
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    retry_read:
    if(i2cProcessHandler::i2cReadHandler(I2CInfo,0x61,sizeof(uint8_t),requestDownloadRespData,sizeof(requestDownloadRespData)/sizeof(uint8_t))) {
        if(requestDownloadRespDate[1]==0x00){//OK
            LOG_D("主机通过 [0x60 0x60] 通知 display 开始下载成功\n");
        } else if(requestDownloadRespDate[1]==0x02){//BUSY 
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            goto retry_read;
        } else if(requestDownloadRespDate[1]==0x03){ //RETRANSMISSION
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            goto retry_write;
        }
    }
    return 0;
}

//Erase Range Command (0x60, 0x81)
int UpgradeProcess::upgradeStep4()
{ 
    if(!binFileDecode_->isHeaderValid()){
        return -1;
    }
    const BinFileHeader& header = binFileDecode_->getHeader();
    const uint8_t* mcuBlockByteLength = header.blocks[0].length; 
    bool mcuBolckValid{true},touchBolckValid{true},tconBolckValid{true}; 
    //std::vector<uint8_t> mcuStartAddrBytes = convertStartAddressToBytes(mcuStartAddr);
    if(BinFileDecode::isBlockAllZero(header.blocks[0])){
        mcuBolckValid = false;
    }
    if(BinFileDecode::isBlockAllZero(header.blocks[1])){
        touchBolckValid = false;
    }
    if(BinFileDecode::isBlockAllZero(header.blocks[2])){
        tconBolckValid = false;
    }
    if(!mcuBolckValid){
        LOG_E("mcu block is all zero, upgrade failed");
        return -1;
    }
    if(!touchBolckValid){
        LOG_E("touch block is all zero, upgrade failed");
        return -1;
    }
    if(!tconBolckValid){
        LOG_E("tcon block is all zero, upgrade failed");
        return -1;
    }
    // 从设备 屏幕中的 MCU模块地址 0x0000 touch地址 0x8000 tcon地址 0x10000 
    uint8_t  eraseRangeReqParamTemp[] = {0x60,0x81,00,00,00,00,mcuBlockByteLength[3],mcuBlockByteLength[2],mcuBlockByteLength[1],mcuBlockByteLength[0]};
    uint8_t crcBytes[2];
    uint8_t  eraseRangeRespDate[4]={0x0};
    uint16_t crc = CRC16XMODEM::calculate(eraseRangeReqParam + 1, sizeof(eraseRangeReqParam) - 1);
    crcBytes[0] = crc & 0xFF;
    crcBytes[1] = (crc >> 8) & 0xFF;
    uint8_t  eraseRangeReqParam[] = {0x60,0x81,00,00,00,00,mcuBlockByteLength[3],mcuBlockByteLength[2],mcuBlockByteLength[1],mcuBlockByteLength[0],crcBytes[0],crcBytes[1]};

    retry_write:
    if (i2cProcessHandler::i2cWriteHandler(I2CInfo,eraseRangeReqParam,sizeof(eraseRangeReqParam)/sizeof(uint8_t))) {
        LOG_D("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",I2CInfo.i2cDevAddr);
        return -1;
    } else {
        LOG_E("主机通过 [0x60 0x81] 通知display擦除指定范围失败\n");
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    retry_read:
    if(i2cProcessHandler::i2cReadHandler(I2CInfo,0x61,sizeof(uint8_t),eraseRangeRespDate,sizeof(eraseRangeRespDate)/sizeof(uint8_t))) {
        if(eraseRangeRespDate[1]==0x00){

        } else if(eraseRangeRespDate[1]==0x00){//OK
            
        } else if(eraseRangeRespDate[1]==0x02){//BUSY 
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            goto retry_read;

        } else if(eraseRangeRespDate[1]==0x03){ //RETRANSMISSION
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            goto retry_write;
        }
    }
    return 0;
}
 //step3
 //主机发Read Session(0x40 0x22 0xFEF7)给 display，读取当前session
int UpgradeProcess::upgradeStep3()
{
   
    uint8_t sessionReqParam[] = {0x40,0x22,0xFE,0xF7}; 
    uint8_t sessionRespData[4]={0x0};
    if (i2cProcessHandler::i2cWriteHandler(I2CInfo,upgradeReqParam,sizeof(upgradeReqParam)/sizeof(uint8_t))) {
        LOG_D("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",I2CInfo.i2cDevAddr);
    } else {
        LOG_E("主机通过 [0x40 0x22 0xFEF7] 通知 display 读取当前session失败\n");
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (i2cProcessHandler::i2cReadHandler(I2CInfo,0x41,sizeof(uint8_t),sessionRespData,sizeof(sessionRespData)/sizeof(uint8_t))) {
        LOG_D("i2c read succeeded,sessionRespData[0]=%#x,sessionRespData[1]=%#x\n",sessionRespData[0],sessionRespData[1]);
        if(sessionRespData[3] == 0x01) {
        //当前在APP模式
        //主机通过Programming Session(0x40 0x10) 命令通知对应的display切到bootloader 刷机模式
            uint8_t progSessionParam[] = {0x40,0x10};
            uint8_t progSessionRespData[3]={0x0};
            if (i2cProcessHandler::i2cWriteHandler(I2CInfo,progSessionParam,sizeof(progSessionParam)/sizeof(uint8_t))) {
                LOG_D("i2cWrite i2caddr:%2x  succeeded\n",I2CInfo.i2cDevAddr);
            } else {
                LOG_E("主机通过Programming Session(0x40 0x10) 命令通知对应的display切到bootloader 刷机模式失败\n");
                return -1;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if (i2cProcessHandler::i2cReadHandler(I2CInfo,0x41,sizeof(uint8_t),progSessionRespData,sizeof(progSessionRespData)/sizeof(uint8_t))) {
            LOG_D("i2c read succeeded,progSessionRespData[0]=%#x,progSessionRespData[1]=%#x\n",progSessionRespData[0],progSessionRespData[1]);
                if(progSessionRespData[0]==0x7F){
                    LOG_E("主机通过Programming Session [0x40 0x10] 读取display 回复0x7F,失败\n");
                    ::write(curOpenFd_, "主机通过Programming Session[0x40 0x10] 读取display 回复0x7F,失败\n", bufsize);
                }

            } else {
                LOG_E("主机通过Programming Session(0x40 0x10) 命令通知对应的display切到bootloader 刷机模式失败\n");
                return -1;
             }



        }
    
    }
}
//step2
//查询display的PN
bool UpgradeProcess::upgradeStep2()
{
    //step2  
    //主机发Read PN命令(0x40 0x22 0xFEF5)给 display，请求读取显示屏的零件号
     uint8_t pnReqParam[] = {0x40,0x22,0xFE,0xF5}; 
     uint8_t  pnRespData[23]={0x0};
     if (i2cProcessHandler::i2cWriteHandler(I2CInfo,pnReqParam,sizeof(pnReqParam)/sizeof(uint8_t))) {
         LOG_D("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",I2CInfo.i2cDevAddr);
     } else {
         LOG_E("主机通过(0x40 0x22 0xFEF5)通知读取显示屏的零件号失败\n");
     }
     //50ms后主机通过IIC协议0x41查询display的PN
     std::this_thread::sleep_for(std::chrono::milliseconds(50));
     if (i2cProcessHandler::i2cReadHandler(I2CInfo,pnReqParam[0],sizeof(pnReqParam[0]),pnRespData,sizeof(pnRespData)/sizeof(uint8_t))) {
        LOG_D("i2cread succeeded,readPNData[0]=%#x,readPNData[1]=%#x\n",readPNData[0],readPNData[1]);
        if(pnRespData[1]==0x7F){
            LOG_E("用户当前升级屏升级失败\n");
            ::write(curOpenFd_, "byte[0]-0x7F,upgrade failed\n", bufsize);
        }else {
            std::string displayPN{};
            for (int i = pNDataStart; i < pNDataStart+pNDataLength; i++) {
                displayPN += static_cast<char>(readPNData[i]);
            }
            //主机的bin文件名称
            std::string hostBinFile = getBinFileFromUpgradeQueue();
            return decodeBinFile(hostBinFile,displayPN);
        }
     }
}
 //step1
//主机通过(0x40 0x22 0xFEF3)通知读取显示屏的I2C通信协议版本
int UpgradeProcess::upgradeStep1()
{
    uint8_t i2cVerReqParam[] = {0x40,0x22,0xFE,0xF3};  
    uint8_t i2cVerRespData[13] = {0x0};
     if (i2cProcessHandler::i2cWriteHandler(I2CInfo,i2cVerReqParam,sizeof(i2cVerReqParam)/sizeof(uint8_t))) {
         LOG_D("i2cWrite i2caddr:%#x regaddr:%#x succeeded\n",I2CInfo.i2cDevAddr,writeNotifyData[0]);
     } else {
         LOG_E("主机通过(0x40 0x22 0xFEF3)通知读取显示屏的I2C通信协议版本失败\n");
         return -1;
     }
     std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (i2cProcessHandler::i2cReadHandler(I2CInfo,0x41,sizeof(uint8_t),i2cVerRespData,sizeof(i2cVerRespData)/sizeof(uint8_t))) {
          LOG_D("i2cread succeeded,i2cVerRespData[0]=%#x,i2cVerRespData[1]=%#x\n",i2cVerRespData[0],i2cVerRespData[1]);
          std::string i2cVersion;
          for (int i = 3; i < 13; i++) {
              i2cVersion += static_cast<char>(i2cVerRespData[i]);
          }
          LOG_D("I2C协议版本: %s", i2cVersion.c_str());
    } else {
          LOG_E("i2cread failed\n");
          return -1;
    }
}

