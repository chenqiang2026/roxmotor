#ifndef UPGRADE_WORKER_H
#define UPGRADE_WORKER_H
#include "../SafeQueue.h"
#include "ScreenCommand.h"
#include <thread>
#include <atomic>
#include <functional>
#include "../UpgradeProcess.h"

class UpgradeCmdExecutor {
public:
    explicit UpgradeCmdExecutor(std::shared_ptr<UpgradeProcess> upgradeProcess) ;
   // ~UpgradeCmdExecutor() = default;
   ~UpgradeCmdExecutor();
    void start(std::shared_ptr<SafeQueue<ScreenCommand>> upgradeQueue);
    void stop(); 
private:
    void run();
private:
    std::shared_ptr<UpgradeProcess> upgradeProcess_;
    std::shared_ptr<SafeQueue<ScreenCommand>> queue_{};
    std::thread upgradeCmdThread_;
    std::atomic<bool> running_{false};

};

#endif //UPGRADE_WORKER_H
