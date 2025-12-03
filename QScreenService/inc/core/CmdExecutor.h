#ifndef CMD_EXECUTOR_H
#define CMD_EXECUTOR_H
#include "../ScreenDeviceController.h"
#include "../SafeQueue.h"
#include "ScreenCommand.h"
#include <thread>
#include <atomic>


class CmdExecutor
{
public:
    explicit CmdExecutor(std::shared_ptr<ScreenDeviceController> screenDeviceController);

    ~CmdExecutor();
    void start(std::shared_ptr<SafeQueue<ScreenCommand>> safeQueue);
    void stop();
private:
    void run();    
private:
    std::atomic<bool> running_{false};
    std::shared_ptr<ScreenDeviceController> screenDeviceController_;
    std::thread cmdFromQueueThread_;
    std::shared_ptr<SafeQueue<ScreenCommand>> cmdQueue_;

};

#endif //CMD_EXECUTOR_H
