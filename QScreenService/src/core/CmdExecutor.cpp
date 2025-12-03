#include "../../inc/core/CmdExecutor.h"
#include "../../inc/Config.h"
#include "../../inc/I2cProcessHandler.h"
#include <unistd.h>

CmdExecutor::CmdExecutor(std::shared_ptr<ScreenDeviceController> screenDeviceController)
    : screenDeviceController_(std::move(screenDeviceController))
{
}

CmdExecutor::~CmdExecutor()
{
}

void CmdExecutor::start(std::shared_ptr<SafeQueue<ScreenCommand>> safeQueue)
{
    if(running_) return;
    cmdQueue_ = safeQueue;
    running_ = true;
    cmdFromQueueThread_ = std::thread(&CmdExecutor::run,this);   
} 
void CmdExecutor::stop()
{
    if(!running_.load()) return;
    running_.store(false);
    if(cmdFromQueueThread_.joinable()){
        cmdFromQueueThread_.join();
    }
}

void CmdExecutor::run()
{
    ScreenCommand screenCommand;
    while(running_) {
        if(!cmdQueue_->Pop(screenCommand)) break;
        ScreenType screenType = screenCommand.screenType;
        ScreenCommand::Type cmdType = screenCommand.type;
        int fd = screenCommand.replyFd;
        screenDeviceController_->getLcdCommandResp(screenType,fd,cmdType);
    }
}
