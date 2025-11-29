#include "ScreenDeviceController.h"
#include <cstring>

using namespace std;
struct i2cDriverInfo;
constexpr const char* LOG_TAG = "ScreenDeviceController";

//lcd 命令
constexpr const char*  lcdcommandSver = "lcdgetsuppliersver";
constexpr const char*  lcdcommandHver = "lcdgetsupplierhver";
constexpr const char*  lcdcommandBlver = "lcdgetsupplierblver";
constexpr const char*  lcdcommandTpver = "lcdgetsuppliertpver";
constexpr const char*  lcdcommandBin = "lcdupdate-p";
constexpr const char*  lcdcommandTpbin = "lcdupdatetp-p";
ScreenDeviceController::ScreenDeviceController(const i2cDriverInfo& i2cInfo) 
    : i2cInfo_(i2cInfo) {}

int ScreenDeviceController::lcdGetSver(const  i2cDriverInfo &I2CInfo)
{
     //  lcd get supplier sver（软件版本）
     uint8_t sverReqparam[] = {0x40,0x22,0xFE,0xF0};
     uint8_t sverRespData[23] = {0x0};
     std::string sverRespDataStr = "";
     //i2c write
     if (i2cProcessHandler::i2cWriteHandler(I2CInfo,sverReqparam,sizeof(sverReqparam)/sizeof(uint8_t))) {
         LOG_D("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",I2CInfo.i2cDevAddr,sverReqparam[0]);
     } else {
         LOG_E("i2cWrite i2caddr:%2x regaddr:%2x failed\n",I2CInfo.i2cDevAddr,sverReqparam[0]);
         return -1;
     }
     std::this_thread::sleep_for(std::chrono::milliseconds(50));
     if(i2cProcessHandler::i2cReadHandler(I2CInfo,0x41,sizeof(uint8_t),sverRespData,sizeof(sverRespData)/sizeof(uint8_t))) {
        LOG_D("i2cRead i2caddr:%2x regaddr:%2x succeeded\n",I2CInfo.i2cDevAddr,0x41);
        for (int i = 3; i < 23; i++) {
             //将从3到23 的sver 转换成字符串
            sverRespDataStr += static_cast<char>(sverRespData[i]);
        }
        if (devScreenIviFd_ == -1) {
            LOG_E("屏幕FD 未初始化，无法缓存命令结果 %s:%d", __FUNCTION__, __LINE__);
            return -1;
        }
        auto it = lcdCommandMap_.find(devScreenIviFd_);
        if(it==lcdCommandMap_.end()){
            LOG_E("屏幕 FD 未在 lcdCommandMap_ 中注册 %s:%d", __FUNCTION__, __LINE__);
            return -1;    
        }
        it->second.emplace(lcdCommandSver, sverRespDataStr);
     } else {
         LOG_E("i2cRead i2caddr:%2x regaddr:%2x failed\n",I2CInfo.i2cDevAddr,0x41);
         return -1;
     }
    return 0;
}

int ScreenDeviceController::lcdGetHver(const  i2cDriverInfo &I2CInfo)
{
     //  lcd get supplier hver（硬件版本）
     uint8_t hverReqparam[] = {0x40,0x22,0xFE,0xF1};
     uint8_t hverRespData[23] = {0x0};
     std::string hverRespDataStr = "";
     if (i2cProcessHandler::i2cWriteHandler(I2CInfo,sverReqparam,sizeof(sverReqparam)/sizeof(uint8_t))) {
         LOG_D("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",I2CInfo.i2cDevAddr,sverReqparam[0]);
     } else {
         LOG_E("i2cWrite i2caddr:%2x regaddr:%2x failed\n",I2CInfo.i2cDevAddr,sverReqparam[0]);
         return -1;
     }
     std::this_thread::sleep_for(std::chrono::milliseconds(50));
     if(i2cProcessHandler::i2cReadHandler(I2CInfo,0x41,sizeof(uint8_t),hverRespData,sizeof(hverRespData)/sizeof(uint8_t))) {
        LOG_D("i2cRead i2caddr:%2x regaddr:%2x succeeded\n",I2CInfo.i2cDevAddr,0x41);
        for (int i = 3; i < 23; i++) {
             //将从3到23 的sver 转换成字符串
            hverRespDataStr += static_cast<char>(hverRespData[i]);
        }
        if (devScreenIviFd_ == -1) {
            LOG_E("屏幕FD 未初始化，无法缓存命令结果 %s:%d", __FUNCTION__, __LINE__);
            return -1;
        }
        auto it = lcdCommandMap_.find(devScreenIviFd_);
        if(it==lcdCommandMap_.end()){
            LOG_E("屏幕 FD 未在 lcdCommandMap_ 中注册 %s:%d", __FUNCTION__, __LINE__);
            return -1;    
        }
        it->second.emplace(lcdCommandHver, hverRespDataStr);
     } else {
         LOG_E("i2cRead i2caddr:%2x regaddr:%2x failed\n",I2CInfo.i2cDevAddr,0x41);
         return -1;
     }
    return 0;
}

int ScreenDeviceController::lcdGetBlver(const  i2cDriverInfo &I2CInfo)
{
     //  lcd get supplier blver（bootload版本）
    uint8_t blverReqparam[] = {0x40,0x22,0xFE,0xF2};
    uint8_t blverRespData[23] = {0x0};
    std::string blverRespDataStr = "";
    if (i2cProcessHandler::i2cWriteHandler(I2CInfo,blverReqparam,sizeof(blverReqparam)/sizeof(uint8_t))) {
        LOG_D("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",I2CInfo.i2cDevAddr,sverReqparam[0]);
     } else {
        LOG_E("i2cWrite i2caddr:%2x regaddr:%2x failed\n",I2CInfo.i2cDevAddr,sverReqparam[0]);
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if(i2cProcessHandler::i2cReadHandler(I2CInfo,0x41,sizeof(uint8_t),blverRespData,sizeof(blverRespData)/sizeof(uint8_t))) {
        LOG_D("i2cRead i2caddr:%2x regaddr:%2x succeeded\n",I2CInfo.i2cDevAddr,0x41);
        for (int i = 3; i < 23; i++) {
             //将从3到23 的sver 转换成字符串
            blverRespDataStr += static_cast<char>(blverRespData[i]);
        }
        if (devScreenIviFd_ == -1) {
            LOG_E("屏幕FD 未初始化，无法缓存命令结果 %s:%d", __FUNCTION__, __LINE__);
            return -1;
        }
        auto it = lcdCommandMap_.find(devScreenIviFd_);
        if(it==lcdCommandMap_.end()){
            LOG_E("屏幕 FD 未在 lcdCommandMap_ 中注册 %s:%d", __FUNCTION__, __LINE__);
            return -1;    
        }
        it->second.emplace(lcdCommandBlver, blverRespDataStr);
     } else {
        LOG_E("i2cRead i2caddr:%2x regaddr:%2x failed\n",I2CInfo.i2cDevAddr,0x41);
        return -1;
    }
    return 0;
}

int ScreenDeviceController::lcdGetTpver(const  i2cDriverInfo &I2CInfo) {
     //  lcd get supplier tp（tp版本）
    uint8_t tpverReqparam[] = {0x40,0x22,0xFE,0xFB};
    uint8_t tpverRespData[13] = {0x0};
    std::string tpverRespDataStr = "";
    if (i2cProcessHandler::i2cWriteHandler(I2CInfo,tpverReqparam,sizeof(tpverReqparam)/sizeof(uint8_t))) {
        LOG_D("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",I2CInfo.i2cDevAddr,tpverReqparam[0]);
    } else {
        LOG_E("i2cWrite i2caddr:%2x regaddr:%2x failed\n",I2CInfo.i2cDevAddr,tpverReqparam[0]);
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if(i2cProcessHandler::i2cReadHandler(I2CInfo,0x41,sizeof(uint8_t),tpverRespData,sizeof(tpverRespData)/sizeof(uint8_t))) {
        LOG_D("i2cRead i2caddr:%2x regaddr:%2x succeeded\n",I2CInfo.i2cDevAddr,0x41);
        for (int i = 3; i < 13; i++) {
             //将从3到13 的sver 转换成字符串
            tpverRespDataStr += static_cast<char>(tpverRespData[i]);
        }
        if (devScreenIviFd_ == -1) {
            LOG_E("屏幕FD 未初始化，无法缓存命令结果 %s:%d", __FUNCTION__, __LINE__);
            return -1;
        }
        auto it = lcdCommandMap_.find(devScreenIviFd_);
        if(it==lcdCommandMap_.end()){
            LOG_E("屏幕 FD 未在 lcdCommandMap_ 中注册 %s:%d", __FUNCTION__, __LINE__);
            return -1;    
        }
        it->second.emplace(lcdCommandTpver, tpverRespDataStr);
    } else {
        LOG_E("i2cRead i2caddr:%2x regaddr:%2x failed\n",I2CInfo.i2cDevAddr,0x41);
        return -1;
    }
    return 0;

}
