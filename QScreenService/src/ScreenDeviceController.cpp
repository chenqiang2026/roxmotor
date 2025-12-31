#include "../inc/ScreenDeviceController.h"
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <thread>
#include <chrono> 
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include "../inc/Config.h"
#include "../inc/Logger.h"
#include "../inc/utils/StringUtil.h"

constexpr const char* LOG_TAG = "ScreenDeviceController";

//lcd 命令
constexpr const char*  lcdcommandSver = "lcdgetsuppliersver";
constexpr const char*  lcdcommandHver = "lcdgetsupplierhver";
constexpr const char*  lcdcommandBlver = "lcdgetsupplierblver";
constexpr const char*  lcdcommandTpver = "lcdgetsuppliertpver";
constexpr const char*  lcdcommandBin = "lcdupdate-p";
constexpr const char*  lcdcommandTpbin = "lcdupdatetp-p";
// ScreenDeviceController::ScreenDeviceController(const i2cDriverInfo& i2cInfo) 
//     : i2cInfo_(i2cInfo) {}

std::string ScreenDeviceController::getLcdCommandResp(ScreenType screenType,int replyFd,ScreenCommand::Type cmdType)
{
    std::string commandResp;
    i2cDriverInfo i2cInfo;
    i2cInfo.i2cDevPath = "";
    i2cInfo.i2cFd = -1;
    i2cInfo.i2cDevAddr = -1;
    if(screenType == ScreenType::Cluster) {
        i2cInfo.i2cDevPath = Config::i2cDevCluster();
        i2cInfo.i2cDevAddr = Config::I2C_ADDR_CLUSTER;
    } else if (screenType == ScreenType::IVI) {
        i2cInfo.i2cDevPath =  Config::i2cDevIVI();
        i2cInfo.i2cDevAddr = Config::I2C_ADDR_IVI;
    } else if (screenType == ScreenType::Ceiling) {
        i2cInfo.i2cDevPath =  Config::i2cDevCeiling();
        i2cInfo.i2cDevAddr = Config::I2C_ADDR_CEILING;
    }
        
    i2cInfo.i2cFd = i2cProcessHandler::i2cInitHandler(i2cInfo.i2cDevPath);

    if(i2cInfo.i2cFd == -1) {
        commandResp = "CmdExecutor::run() i2c init failed";
        ::write(replyFd, commandResp.c_str(), commandResp.size());
        return commandResp;
    }

    switch(cmdType) {
        case  ScreenCommand::Type::GetSver:
            commandResp = getSver(i2cInfo);
            ::write(replyFd, commandResp.c_str(), commandResp.size());
            break;
        case  ScreenCommand::Type::GetHver:
            commandResp = getHver(i2cInfo);
            ::write(replyFd, commandResp.c_str(), commandResp.size());
            break;
        case  ScreenCommand::Type::GetBlver:
            commandResp = getBlver(i2cInfo);
            ::write(replyFd, commandResp.c_str(), commandResp.size());
            break;
        case  ScreenCommand::Type::GetTpver:
            commandResp = getTpver(i2cInfo);
            ::write(replyFd, commandResp.c_str(), commandResp.size());
            break;
        case  ScreenCommand::Type::UpgradeP:
            //commandResp = upgradePBin(i2cInfo);
            break;
        case  ScreenCommand::Type::UpgradeTp:
            // commandResp = upgradeTPBin(i2cInfo);
            break;
        default:
            commandResp = "CmdExecutor::run() unknown command type";
            break;
    }
    i2cProcessHandler::i2cDeinitHandler(i2cInfo.i2cFd);
}

// lcd get supplier sver（软件版本）
std::string ScreenDeviceController::getSver(const i2cDriverInfo& i2cInfo) 
{
    uint8_t sverReqparam[] = {0x40,0x03,0x22,0xFE,0xFD};
    uint8_t sverRespData[14] = {0x0};
    std::string sverRespDataStr{};
    //std::stringstream ss;
    //i2c write
    if (i2cProcessHandler::i2cWriteHandler(i2cInfo,sverReqparam,sizeof(sverReqparam)/sizeof(uint8_t))) {
        SCREEN_LOGD("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,sverReqparam[0]);
        std::cout << "i2cWrite i2caddr:" << std::hex << (int)i2cInfo.i2cDevAddr << " regaddr:" << std::hex << (int)sverReqparam[0] << " succeeded" << std::endl;
    } else {
        SCREEN_LOGE("i2cWrite i2caddr:%2x regaddr:%2x failed\n",i2cInfo.i2cDevAddr,sverReqparam[0]);   
        std::cout << "i2cWrite i2caddr:" << std::hex << (int)i2cInfo.i2cDevAddr << " regaddr:" << std::hex << (int)sverReqparam[0] << " failed" << std::endl;
        return nullptr;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if(i2cProcessHandler::i2cReadHandler(i2cInfo,0x41,sizeof(uint8_t),sverRespData,sizeof(sverRespData)/sizeof(uint8_t))) {
        std::cout<<"ScreenController i2cReadHandler OK "<<std::endl;
        SCREEN_LOGD("i2cRead i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,0x41);
        //std::cout <<"getSver sverRespData[0] :  " <<static_cast<char>(sverRespData[0]) <<std::endl;
        //std::cout <<"getSver sverRespData[1] :  " <<static_cast<char>(sverRespData[1]) <<std::endl;
        std::cout<<"----------------------"<<std::endl;
        //std::cout <<"getSver sverRespData[1] :  " <<static_cast<uint8_t>(sverRespData[1]) <<std::endl;
        //std::cout <<"getSver sverRespData[1] :  " <<static_cast<uint8_t>(sverRespData[1]) <<std::endl;
        sverRespDataStr = StringUtil::hexToSVer(sverRespData+4,10);
      //  for (int i = 4; i < 14; i++) {
            //将从3到23 的sver 转换成字符串
            //char c = static_cast<char>(sverRespData[i]);
           // uint8_t sdec = static_cast<char>(sverRespData[i]);
           // printf("0x%02x ",sdec);
            //sverRespDataStr += static_cast<uint8_t>(sverRespData[i]);
            //std::cout << "sverRespData char["<<std::dec<<i<<"]:"<<c<<std::endl;
            //std::cout<<"sverRespData char型["<<std::dec<<i<<"]:"<<static_cast<char>(sverRespData[i])<<std::endl;
            //std::cout<<"sverRespData uint8_t型["<<std::dec<<i<<"]:"<<static_cast<uint8_t>(sverRespData[i])<<std::endl;
            //ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(sverRespData[i]);
            //ss << " ";  // 字节间用空格分隔        
            //sverRespDataStr += sdec;
        //}  
        std::cout<<"ScreenDeviceController::getSver::sverRespData:"<<sverRespDataStr <<std::endl;
    }
    return sverRespDataStr;
}

// lcd get supplier hver（硬件版本）
std::string ScreenDeviceController::getHver(const i2cDriverInfo& i2cInfo)
{   
    uint8_t hverReqparam[] = {0x40,0x03,0x22,0xFE,0xFE};
    uint8_t hverRespData[14] = {0x0};
    std::string hverRespDataStr = "";
    if (i2cProcessHandler::i2cWriteHandler(i2cInfo,hverReqparam,sizeof(hverReqparam)/sizeof(uint8_t))) {
        SCREEN_LOGD("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,hverReqparam[0]);
        std::cout << "i2cWrite i2caddr:" << std::hex << (int)i2cInfo.i2cDevAddr << " regaddr:" << std::hex << (int)hverReqparam[0] << " succeeded" << std::endl;
    } else {
        SCREEN_LOGE("i2cWrite i2caddr:%2x regaddr:%2x failed\n",i2cInfo.i2cDevAddr,hverReqparam[0]);
        std::cout << "i2cWrite i2caddr:" << std::hex << (int)i2cInfo.i2cDevAddr << " regaddr:" << std::hex << (int)hverReqparam[0] << " failed" << std::endl;
        return nullptr;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if(i2cProcessHandler::i2cReadHandler(i2cInfo,0x41,sizeof(uint8_t),hverRespData,sizeof(hverRespData)/sizeof(uint8_t))) {
        //LOG_D("i2cRead i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,0x41);
        hverRespDataStr = StringUtil::hexToHVer(hverRespData+4,10);
        //for (int i = 4; i < 14; i++) {
            //char c = static_cast<char>(hverRespData[i]);
           // uint8_t hdec = static_cast<uint8_t>(hverRespData[i]);
            //printf("0x%02x ",hdec);
            //std::cout << "hverRespData char["<<std::dec<<i<<"]:"<< c <<std::endl;
            //SCREEN_LOGD("hverRespData[%d]: d%02X\n", i, hverRespData[i]);
            //std::cout << "hverRespData[" << std::dec << i << "]:" << c << std::endl;
            //将从3到23 的sver 转换成字符串
            //hverRespDataStr += hdec;
        //} 
        std::cout << "ScreenDeviceController::getHver::hverRespDataStr:" << hverRespDataStr << std::endl;
    }
    return hverRespDataStr;
}

// lcd get supplier blver（bootload版本）
std::string ScreenDeviceController::getBlver(const i2cDriverInfo& i2cInfo)
{
    uint8_t blverReqparam[] = {0x40,0x03,0x22,0xFE,0xF2};
    uint8_t blverRespData[24] = {0x0};
    std::string blverRespDataStr = "";
    if (i2cProcessHandler::i2cWriteHandler(i2cInfo,blverReqparam,sizeof(blverReqparam)/sizeof(uint8_t))) {
        SCREEN_LOGD("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,blverReqparam[0]);
        std::cout << "i2cWrite i2caddr:" << std::hex << (int)i2cInfo.i2cDevAddr << " regaddr:" << std::hex << (int)blverReqparam[0] << " succeeded" << std::endl;
    } else {
        SCREEN_LOGE("i2cWrite i2caddr:%2x regaddr:%2x failed\n",i2cInfo.i2cDevAddr,blverReqparam[0]);
        std::cout << "i2cWrite i2caddr:" << std::hex << (int)i2cInfo.i2cDevAddr << " regaddr:" << std::hex << (int)blverReqparam[0] << " failed" << std::endl;
        return nullptr;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if(i2cProcessHandler::i2cReadHandler(i2cInfo,0x41,sizeof(uint8_t),blverRespData,sizeof(blverRespData)/sizeof(uint8_t))) {
        //LOG_D("i2cRead i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,0x41);
        blverRespDataStr = StringUtil::hexToBlVer(blverRespData+4,20);
        //for (int i = 4; i < 24; i++) {
             //将从3到23 的sver 转换成字符串
            //std::cout << "blverRespData[" << std::dec << i << "]:" << std::hex << (int)blverRespData[i] << std::endl;
            //char c = static_cast<char>(blverRespData[i]);
            //std::cout << "blverRespData char["<<std::dec<<i<<"]:"<<c<<std::endl;
            //blverRespDataStr += c;
        //}
        std::cout << "ScreenDeviceController::getBlver::blverRespDataStr:" << blverRespDataStr << std::endl;
    }
    return blverRespDataStr;
}

// lcd get supplier tp（tp版本）
std::string ScreenDeviceController::getTpver(const i2cDriverInfo& i2cInfo) 
{ 
    uint8_t tpverReqparam[] = {0x40,0x03,0x22,0xFE,0xFB};
    uint8_t tpverRespData[14] = {0x0};
    std::string tpverRespDataStr = "";
    if (i2cProcessHandler::i2cWriteHandler(i2cInfo,tpverReqparam,sizeof(tpverReqparam)/sizeof(uint8_t))) {
        SCREEN_LOGD("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,tpverReqparam[0]);
        std::cout << "i2cWrite i2caddr:" << std::hex << (int)i2cInfo.i2cDevAddr << " regaddr:" << std::hex << (int)tpverReqparam[0] << " succeeded" << std::endl;
    } else {
        SCREEN_LOGE("i2cWrite i2caddr:%2x regaddr:%2x failed\n",i2cInfo.i2cDevAddr,tpverReqparam[0]);
        std::cout << "i2cWrite i2caddr:" << std::hex << (int)i2cInfo.i2cDevAddr << " regaddr:" << std::hex << (int)tpverReqparam[0] << " failed" << std::endl;
        return nullptr;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if(i2cProcessHandler::i2cReadHandler(i2cInfo,0x41,sizeof(uint8_t),tpverRespData,sizeof(tpverRespData)/sizeof(uint8_t))) {
        tpverRespDataStr = StringUtil::hexToTpVer(tpverRespData+4,10);
        //LOG_D("i2cRead i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,0x41);
        // for (int i = 4; i < 14; i++) {
        //      //将从3到13 的sver 转换成字符串
        //     //std::cout << "tpverRespData[" << std::dec << i << "]:" << std::hex << (int)tpverRespData[i] << std::endl;
        //     char c = static_cast<char>(tpverRespData[i]);
        //     std::cout << "tpverRespData char["<<std::dec<<i<<"]:"<<c<<std::endl;
        //     tpverRespDataStr += c;
        // }
        std::cout << "ScreenDeviceController::getTpver::tpverRespDataStr:" << tpverRespDataStr << std::endl;
    }  
    return tpverRespDataStr;
}
