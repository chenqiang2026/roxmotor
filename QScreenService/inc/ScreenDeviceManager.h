#ifndef DEV_MANAGER_H
#define DEV_MANAGER_H

#include <string>
#include <unordered_map>
#include <poll.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include "ScreenDeviceController.h"
enum class ScreenType {
    Cluster,
    Ivi,
    Ceiling
};
struct ScreenCommand {
    ScreenType screenType; 
    std::string command;  
};

class ScreenDeviceManager {
public:
    //static ScreenDeviceManager& getInstance();
    ~ScreenDeviceManager();
    ScreenDeviceManager(const ScreenDeviceManager&) = delete;
    ScreenDeviceManager& operator=(const ScreenDeviceManager&) = delete;

    int initScreenDevFd();
    void stopPolling();
    int getScreenFd(ScreenType type) const;
    const std::unordered_map<int, ScreenType>& getFdToScreenTypeMap() const { return fdToScreenTypeMap_; }

protected:
    ScreenDeviceManager(); 
    int pollScreenDevFd();
    void closeAllFds();
    void processCommands();
    int readData(ScreenType screenType, int curOpenScreenFd_, char* buf, int bufSize);
    int processScreenCommand(const std::string& screenCommand);
    void popScreenCommand(ScreenCommand& cmd);

protected:    
    int curOpenScreenFd_{-1};
    int screenDevClusterFd_{-1};
    int screenDevIviFd_{-1};
    int screenDevCeilingFd_{-1};
    std::vector<std::string> lcdCommandList_{
        "lcdgetsuppliersver",
        "lcdgetsupplierhver",
        "lcdgetsupplierblver",
        "lcdgetsuppliertpver",
        "lcdupdate-p",
        "lcdupdatetp-p",
    };   
    // FD到屏幕类型的映射
    std::unordered_map<int, ScreenType> fdToScreenTypeMap_{}; 
    // 轮询线程和控制标志
    std::thread pollThread_;
    std::atomic<bool> isPolling_{false};
    std::unique_ptr<ScreenDeviceController> screenDeviceController_;
};
#endif // DEV_MANAGER_H
