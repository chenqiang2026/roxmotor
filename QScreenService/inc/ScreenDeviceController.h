#ifndef SCREEN_DEVICE_CONTROLLER_H
#define SCREEN_DEVICE_CONTROLLER_H
#include <string>
#include "core/ScreenCommand.h"
#include "I2cProcessHandler.h"

class ScreenDeviceController {
public:
    // 构造函数接收I2C设备信息
    //explicit ScreenDeviceController(const i2cDriverInfo& i2cInfo);
    ScreenDeviceController()=default;
    ~ScreenDeviceController()=default;

    std::string getLcdCommandResp(ScreenType screenType,int replyFd,ScreenCommand::Type cmdType);

private:
    // 版本查询方法
    std::string getSver(const i2cDriverInfo& i2cInfo);
    std::string getHver(const i2cDriverInfo& i2cInfo); 
    std::string getBlver(const i2cDriverInfo& i2cInfo); 
    std::string getTpver(const i2cDriverInfo& i2cInfo); 
    //void setCommandQueue(CommandQueue* queue);
    //void startProcessing();
    //void stopProcessing();
    // 升级相关方法
    // int upgradePBin(); 
    // int upgradeTPBin();   
};

#endif //SCREEN_DEVICE_CONTROLLER_H
