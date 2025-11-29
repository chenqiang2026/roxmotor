#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include "ScreenDeviceManager.h"
#include "SafeQueue.h" 

// 命令结构
struct DeviceCommand {
    std::string cmd;
    ScreenType targetType;
    bool isUpgrade;
};

class CommandProcessor : public ScreenDeviceManager {
public:
    CommandProcessor() = default;
    ~CommandProcessor();
    void start();
    void stop();
    void enqueueCommand(const DeviceCommand& cmd);

private:
    void processLoop();
    void executeCommand(const DeviceCommand& cmd);

    std::atomic<bool> isProcessing_ = false;
    std::thread processThread_;
    SafeQueue<DeviceCommand> safeQueue_;    
    SafeQueue<DeviceCommand> upgradeQueue_; 
};

#endif // COMMAND_PROCESSOR_H
