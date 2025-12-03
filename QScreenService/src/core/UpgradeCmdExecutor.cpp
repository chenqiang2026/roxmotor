#include "../../inc/core/UpgradeCmdExecutor.h"
#include "../../inc/I2cProcessHandler.h"
#include "../../inc/Config.h"
#include "../../inc/UpgradeProcess.h"
#include <memory>
#include <unistd.h>

//UpgradeCmdExecutor::UpgradeCmdExecutor() = default;
UpgradeCmdExecutor::UpgradeCmdExecutor(std::shared_ptr<UpgradeProcess> upgradeProcess) {
    upgradeProcess_ = std::move(upgradeProcess);
}

UpgradeCmdExecutor::~UpgradeCmdExecutor() {
    stop();
}

void UpgradeCmdExecutor::start(std::shared_ptr<SafeQueue<ScreenCommand>> upgradeQueue) {
    if (running_) {
        return;
    }
    running_ = true;
    queue_ = &upgradeQueue;
    upgradeCmdThread_ = std::thread(&UpgradeCmdExecutor::run, this);
}

void UpgradeCmdExecutor::stop() {
    if (!running_.load()) {
        return;
    }
    if(queue_) queue_->Stop();  
    running_.store(false);
    if(upgradeCmdThread_.joinable()) {
        upgradeCmdThread_.join();
    }   
}

void UpgradeCmdExecutor::run() {
    ScreenCommand screenCmd;
    while (running_) {
        if (!queue_->Pop(screenCmd)) break;
        if(screenCmd.isUpgrade ||screenCmd.finalCmd.empty()) {
            const std::string errresp = " UpgradeCmdExecutor::run  ERR : invalid upgrade command \n";
            ::write(screenCmd.replyFd, errresp.c_str(), errresp.size());
            continue;
        }
        std::string finalCmd = screenCmd.finalCmd;
        std::string binPath = screenCmd.binPath;
        ScreenType screenType = screenCmd.screenType;
        ScreenCommand::Type cmdType = screenCmd.type;
        int replyFd = screenCmd.replyFd;
        if(screenCmd.valid){
            upgradeProcess_->upgrade(screenCmd.binPath);
        }



        std::string i2cDev; uint32_t i2cAddr;
        if(screenCmd.screenType == ScreenType::Cluster) {
            i2cDev =  Config::i2cDevCluster();
            i2cAddr = Config::I2C_ADDR_CLUSTER;
        } else if (screenCmd.screenType == ScreenType::IVI) {
            i2cDev =  Config::i2cDevIVI();
            i2cAddr = Config::I2C_ADDR_IVI;
        } else if (screenCmd.screenType == ScreenType::Ceiling) {
            i2cDev =  Config::i2cDevCeiling();
            i2cAddr = Config::I2C_ADDR_CEILING;
        }
        int i2cFd = i2cProcessHandler::i2cInitHandler(i2cDev);
        if(i2cFd < 0){
            const std::string errresp = " UpgradeCmdExecutor::run  ERR : i2c open failed \n";
            ::write(screenCmd.replyFd, errresp.c_str(), errresp.size());
            continue;
        }
        UpgradeProcess upgradeProcess;
        if(!upgradeProcess.upgrade(screenCmd.binPath)){
            const std::string errresp = " UpgradeCmdExecutor::run  ERR : upgrade failed \n";
            ::write(screenCmd.replyFd, errresp.c_str(), errresp.size());
            continue;
        }
        const std::string resp = " UpgradeCmdExecutor::run  OK : upgrade success \n";
        ::write(screenCmd.replyFd, resp.c_str(), resp.size());
        //::close(screenCmd.replyFd);
        i2cProcessHandler::i2cDeinitHandler(i2cFd);
    }
}
