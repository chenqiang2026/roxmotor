#include <thread>
#include <unistd.h>
#include "../inc/ScreenDeviceController.h"
#include "../inc/Config.h"

using namespace std;
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
std::string ScreenDeviceController::getSver(const i2cDriverInfo& i2cInfo) {
uint8_t sverReqparam[] = {0x40,0x22,0xFE,0xF0};
    uint8_t sverRespData[23] = {0x0};
    std::string sverRespDataStr = "";
    //i2c write
    if (i2cProcessHandler::i2cWriteHandler(i2cInfo,sverReqparam,sizeof(sverReqparam)/sizeof(uint8_t))) {
        LOG_D("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,sverReqparam[0]);
    } else {
        LOG_E("i2cWrite i2caddr:%2x regaddr:%2x failed\n",i2cInfo.i2cDevAddr,sverReqparam[0]);
        return nullptr;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if(i2cProcessHandler::i2cReadHandler(i2cInfo,0x41,sizeof(uint8_t),sverRespData,sizeof(sverRespData)/sizeof(uint8_t))) {
        LOG_D("i2cRead i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,0x41);
        for (int i = 3; i < 23; i++) {
            //将从3到23 的sver 转换成字符串
            sverRespDataStr += static_cast<char>(sverRespData[i]);
        }
    }
    return sverRespDataStr;
}

// lcd get supplier hver（硬件版本）
std::string ScreenDeviceController::getHver(const i2cDriverInfo& i2cInfo)
{   
    uint8_t hverReqparam[] = {0x40,0x22,0xFE,0xF1};
    uint8_t hverRespData[23] = {0x0};
    std::string hverRespDataStr = "";
    if (i2cProcessHandler::i2cWriteHandler(i2cInfo,hverReqparam,sizeof(hverReqparam)/sizeof(uint8_t))) {
        LOG_D("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,hverReqparam[0]);
    } else {
        LOG_E("i2cWrite i2caddr:%2x regaddr:%2x failed\n",i2cInfo.i2cDevAddr,hverReqparam[0]);
        return nullptr;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if(i2cProcessHandler::i2cReadHandler(i2cInfo,0x41,sizeof(uint8_t),hverRespData,sizeof(hverRespData)/sizeof(uint8_t))) {
        LOG_D("i2cRead i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,0x41);
        for (int i = 3; i < 23; i++) {
            //将从3到23 的sver 转换成字符串
            hverRespDataStr += static_cast<char>(hverRespData[i]);
        }  
    }
    return hverRespDataStr;
}

// lcd get supplier blver（bootload版本）
std::string ScreenDeviceController::getBlver(const i2cDriverInfo& i2cInfo)
{
    uint8_t blverReqparam[] = {0x40,0x22,0xFE,0xF2};
    uint8_t blverRespData[23] = {0x0};
    std::string blverRespDataStr = "";
    if (i2cProcessHandler::i2cWriteHandler(i2cInfo,blverReqparam,sizeof(blverReqparam)/sizeof(uint8_t))) {
        LOG_D("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,blverReqparam[0]);
    } else {
        LOG_E("i2cWrite i2caddr:%2x regaddr:%2x failed\n",i2cInfo.i2cDevAddr,blverReqparam[0]);
        return nullptr;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if(i2cProcessHandler::i2cReadHandler(i2cInfo,0x41,sizeof(uint8_t),blverRespData,sizeof(blverRespData)/sizeof(uint8_t))) {
        LOG_D("i2cRead i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,0x41);
        for (int i = 3; i < 23; i++) {
             //将从3到23 的sver 转换成字符串
            blverRespDataStr += static_cast<char>(blverRespData[i]);
        }
    }
    return blverRespDataStr;
}

// lcd get supplier tp（tp版本）
std::string ScreenDeviceController::getTpver(const i2cDriverInfo& i2cInfo) {
    
    uint8_t tpverReqparam[] = {0x40,0x22,0xFE,0xFB};
    uint8_t tpverRespData[13] = {0x0};
    std::string tpverRespDataStr = "";
    if (i2cProcessHandler::i2cWriteHandler(i2cInfo,tpverReqparam,sizeof(tpverReqparam)/sizeof(uint8_t))) {
        LOG_D("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,tpverReqparam[0]);
    } else {
        LOG_E("i2cWrite i2caddr:%2x regaddr:%2x failed\n",i2cInfo.i2cDevAddr,tpverReqparam[0]);
        return nullptr;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if(i2cProcessHandler::i2cReadHandler(i2cInfo,0x41,sizeof(uint8_t),tpverRespData,sizeof(tpverRespData)/sizeof(uint8_t))) {
        LOG_D("i2cRead i2caddr:%2x regaddr:%2x succeeded\n",i2cInfo.i2cDevAddr,0x41);
        for (int i = 3; i < 13; i++) {
             //将从3到13 的sver 转换成字符串
            tpverRespDataStr += static_cast<char>(tpverRespData[i]);
        }
    }  
    return tpverRespDataStr;
}
