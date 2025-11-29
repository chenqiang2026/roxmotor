#ifndef LCD_DEVICE_CONTROLLER_H
#define LCD_DEVICE_CONTROLLER_H

#include "I2cProcessHandler.h"
#include <string>
#include <vector>
#include <map>
#include <unique_ptr>

class ScreenDeviceController {
public:
    // 构造函数接收I2C设备信息
    explicit ScreenDeviceController(const i2cDriverInfo& i2cInfo);
    // 版本查询方法
    int getSver();
    int getHver(); 
    int getBlver();
    int getTpver(); 
    //void setCommandQueue(CommandQueue* queue);
    void startProcessing();
    void stopProcessing();

    // 升级相关方法
    int updatePBin(); 
    int updateTPBin();

private:
    std::unique_ptr<ScreenDeviceManager> deviceManager_;
    i2cDriverInfo i2cInfo_{}; // 保存I2C设备信息
    std::map<int ,std::map<std::string,std::string>> lcdCommandMap_{};//缓存结果 不同屏幕 不同命令
    int devScreenClusterFd_{-1};//dev屏幕节点
    int devScreenIviFd_{-1};
    int devScreenCeilingFd_{-1};
    std::vector<std::string> lcdCommandList_{
        "lcdgetsuppliersver",
        "lcdgetsupplierhver",
        "lcdgetsupplierblver",
        "lcdgetsuppliertpver",
        "lcdupdate-p",
        "lcdupdatetp-p",
    };
   // CommandQueue* commandQueue_ = nullptr;
    std::thread processingThread_;
    std::atomic<bool> isProcessing_{false}; 
    int getScreenFd(ScreenType type) const;
    void processCommands();
    std::string executeCommand(const LcdCommand& cmd);

private:
    std::string getBinFileFromUpgradeQueue();
    int pollQueueLcdcommand()

    
};

#endif // LCD_DEVICE_CONTROLLER_H
