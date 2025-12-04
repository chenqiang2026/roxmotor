#include "ScreenDeviceManager.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include "I2cProcessHandler.h"

// 设备节点路径
static constexpr const char* dev_screen_cluster {"/dev/screen_mcu/panel2-0/info"};
static constexpr const char* dev_screen_ivi {"/dev/screen_mcu/panel0-0/info"};
static constexpr const char* dev_screen_ceiling {"/dev/screen_mcu/panel2-1/info"};

static constexpr int devPollFdNum = 3;
static constexpr int pollTimeout = 100;
static constexpr int bufSize = 128;

ScreenDeviceManager::ScreenDeviceManager() {}

ScreenDeviceManager::~ScreenDeviceManager() {
    stopPolling();
    closeAllFds();
     isProcessing_ = false;
    if (processThread_.joinable()) {
        processThread_.join();
    }
}

ScreenDeviceManager& ScreenDeviceManager::getInstance() {
    static ScreenDeviceManager instance;
    return instance;
}

int ScreenDeviceManager::initScreenDevFd() {
    closeAllFds();
    screenDevClusterFd_ = ::open(dev_screen_cluster, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    screenDevIviFd_ = ::open(dev_screen_ivi, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    screenDevCeilingFd_ = ::open(dev_screen_ceiling, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);

    if (screenDevClusterFd_ < 0 || screenDevIviFd_ < 0 || screenDevCeilingFd_ < 0) {
        LOG_E("打开设备节点失败");
        closeAllFds();
        return -1;
    }
    // 更新FD到屏幕类型的映射
    fdToScreenTypeMap_.clear();
    fdToScreenTypeMap_[screenDevClusterFd_] = ScreenType::Cluster;
    fdToScreenTypeMap_[screenDevIviFd_] = ScreenType::Ivi;
    fdToScreenTypeMap_[screenDevCeilingFd_] = ScreenType::Ceiling;
    // 启动轮询线程
    isPolling_ = true;
    pollThread_ = std::thread(&ScreenDeviceManager::pollScreenDevFd, this);
    sProcessing_ = true;
    processThread_ = std::thread(&ScreenDeviceManager::processCommands, this);
    
    return 0;
}

void ScreenDeviceManager::stopPolling() {
    isPolling_ = false;
    if (pollThread_.joinable()) {
        pollThread_.join();
    }
}

void ScreenDeviceManager::closeAllFds() {
    if (devScreenClusterFd_ != -1) {
        ::close(devScreenClusterFd_);
        devScreenClusterFd_ = -1;
    }
    if (devScreenIviFd_ != -1) {
        ::close(devScreenIviFd_);
        devScreenIviFd_ = -1;
    }
    if (devScreenCeilingFd_ != -1) {
        ::close(devScreenCeilingFd_);
        devScreenCeilingFd_ = -1;
    }
    fdToScreenType_.clear();
}

int ScreenDeviceManager::getScreenFd(ScreenType type) const {
    for (const auto& pair : fdToScreenTypeMap_) {
        if (pair.second == type) {
            return pair.first;
        }
    }
    return -1;
}

int ScreenDeviceManager::pollScreenDevFd() {
    struct pollfd readfds[devPollFdNum] = {0};
    readfds[0].fd = screenDevClusterFd_;
    readfds[0].events = POLLIN;
    readfds[1].fd = screenDevIviFd_;
    readfds[1].events = POLLIN;
    readfds[2].fd = screenDevCeilingFd_;
    readfds[2].events = POLLIN;
    while (isPolling_) {
     int ret = ::poll(readfds, devPollFdNum, pollTimeout);
        if(ret==-1) {
            perror("poll screenDevClusterFd,screenDevIviFd,screenDevCeilingFd error");
            ::close(screenDevClusterFd_);
            ::close(screenDevIviFd_);
            ::close(screenDevCeilingFd_);
            screenDevClusterFd_ = -1;
            screenDevIviFd_ = -1;
            screenDevCeilingFd_ = -1;
            break;
        } else if (ret == 0) {
            continue;;
          } else { 
                char buf[bufSize] = {0};
                for (int i = 0; i < devPollFdNum; ++i) {
                    if (readfds[i].revents & POLLIN) {
                        curOpenScreenFd_ = readfds[i].fd;
                        auto typeIt = fdToScreenTypeMap_.find(curOpenScreenFd_);
                        if (typeIt != fdToScreenTypeMap_.end()) {
                            ScreenType screenType = typeIt->second;
                            readData(screenType,curOpenScreenFd_, buf, bufSize);
                        }
                    }
                    memset(buf, 0, bufSize); 
                } 
            }
    }            
    return 0;
}

int ScreenDeviceManager::readData(ScreenType screenType, int curOpenScreenFd_, char* buf, int bufSize) 
{
   LOG_D("devScreenIviFd has data, reading...");
    int iviRead = ::read(curOpenScreenFd, buf, bufSize);
       
    if (iviRead > 0) {
        std::string iviData(buf, iviRead);
        StringUtil::formatLcdcommand(iviData);
        bool isCommandMatched = false;  
        auto it = std::find_if(lcdCommandList_.begin(), lcdCommandList_.end(),
            [&iviData](const std::string& cmd) {
                return iviData.size() >= cmd.size() &&  
                    iviData.compare(0, cmd.size(), cmd) == 0;
            });
        isCommandMatched = (it != lcdCommandList_.end());
        
        if(isCommandMatched){
            // bool isUpdateCommand = (iviData.substr(0, strlen(lcdcommandBin)) == lcdcommandBin) || 
            //                       (iviData.substr(0, strlen(lcdcommandTpbin)) == lcdcommandTpbin);
            // if(isUpdateCommand){
            //     std::lock_guard<std::mutex> lock(upgradeMutex_);
            //     upgradeQueue_.Push(ScreenCommand{screenType, iviData});
            //     upgradeCv_.notify_one();
            // } else {
                safeQueue_.Push(ScreenCommand{screenType, iviData});
            //}
        }
    } else if (iviRead == -1) {
        ::close(curOpenScreenFd);
        curOpenScreenFd = -1;
        perror("read devScreenIviFd failed,%s %s",__LINE__,__func__);
    }
    
    return iviRead;
}

void ScreenDeviceManager::popScreenCommand() {
    while (isProcessing_) {
        ScreenCommand cmd{};
        if(!safeQueue_.Pop(cmd)){
            continue;
        }  
        formatLcdcommand(cmd.command);
        std::string i2cDevPath = "";
        int i2cFd = -1;
        if(cmd.screenType == ScreenType::Cluster) {
            i2cDevPath = "/dev/i2c7";
            i2cFd = i2cProcessHandler::i2cInitHandler(i2cDevPath);
            curOpenScreenFd_ = devScreenClusterFd_;
        } else if(cmd.screenType == ScreenType::Ivi) {
            curOpenScreenFd_ = devScreenIviFd_;
            i2cFd = i2cProcessHandler::i2cInitHandler(i2cDevPath);
        } else if(cmd.screenType == ScreenType::Ceiling) {
            curOpenScreenFd_ = devScreenCeilingFd_;
            i2cDevPath= "/dev/i2c9"; //(暂不支持)
            i2cFd = i2cProcessHandler::i2cInitHandler(i2cDevPath);
        }
        if(i2cFd == -1){
            LOG_E("i2cFd初始化失败, %s %d, i2cDevPath: %s",__func__,__LINE__,i2cDevPath.c_str());
            continue;
        }
        std::string screenCommand = cmd.command;
        processScreenCommand(screenCommand);

        auto outerIt = lcdCommandMap_.find(curOpenScreenFd_);
        if(outerIt == lcdCommandMap_.end()){
            LOG_E("屏幕FD[%d]未在lcdCommandMap_中找到映射 %s %d",curOpenScreenFd_,__func__,__LINE__);
            continue;
        }
        std::map<std::string ,std::string>& innerMap = outerIt->second;
        auto innerIt = innerMap.find(cmd.command);
        if(innerIt != innerMap.end()){
            std::string lcdCommandResp = innerIt->second;
            LOG_D("屏幕类型[%d]命令[%s]结果: %s", cmd.screenType, cmd.command.c_str(), lcdCommandResp.c_str());
            ssize_t writeRet = ::write(curOpenScreenFd_, lcdCommandResp.c_str(), lcdCommandResp.size());
            if(writeRet == -1) {
                LOG_E("写入屏幕FD[%d]失败: %s, %s,%d", curOpenScreenFd_, strerror(errno), __func__, __LINE__);
            } else if( writeRet!=static_cast<ssize_t>(lcdCommandResp.size())){
                LOG_E("写入屏幕FD[%d]失败: 写入字节数[%ld]与预期字节数[%ld]不一致, %s,%d", curOpenScreenFd_, writeRet, lcdCommandResp.size(), __FUNCTION__, __LINE__);
            } else {
                LOG_D("成功写入屏幕FD[%d] %ld字节数据", curOpenScreenFd_, writeRet);
            }
        }else {
            LOG_E("屏幕FD[%d]命令[%s]未在lcdCommandMap_中找到映射 %s,%d",curOpenScreenFd_,cmd.command.c_str(),__func__,__LINE__);
        }
        std::string lcdUpgradeCommand{};
        std::unique_lock<std::mutex> lock(upgradeMutex_);
        upgradeCv_.wait(lock, [this] { return !upgradeQueue_.IsEmpty(); });
        // 3. 从队列弹出命令（此时队列已非空）
        upgradeQueue_.Pop(lcdUpgradeCommand);
        lock.unlock();
        formatLcdcommand(lcdUpgradeCommand);
            // ======== 2. 解析 -p 后的 bin 文件完整路径 ========
        size_t pPos = slcdCommand.find("-p");
        if (pPos == std::string::npos) {
            LOG_E("%s 格式错误缺少 -p: %s", cmdType.c_str(), lcdCommandResult.c_str());
            return "";
        }
        std::string binFileStart = lcdUpgradeCommand.substr(pPos + 2);
        if (binFileStart.empty()) {
            LOG_E("-p 后路径为空: %s", lcdUpgradeCommand.c_str());
            return "";
        }
    }
}

int ScreenDeviceManager::processScreenCommand(const std::string& screenCommand) {
    if(screenCommand.empty()){
        LOG_E("screenCommand为空 %s %d",__func__,__LINE__);
        return -1;
    }
    if(screenCommand == lcdCommandList_[0]){
        screenDeviceController_->getSver();
        return 0;
    }

    return 0;   
}
