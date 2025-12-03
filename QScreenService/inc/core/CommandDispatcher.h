#ifndef COMMAND_DISPATCHER_H
#define COMMAND_DISPATCHER_H

#include "ScreenCommand.h"
#include <string>
#include "../SafeQueue.h"

class CommandDispatcher
{
public:
    CommandDispatcher();
    ~CommandDispatcher()=default;
    void dispatch(int fd, const std::string& cmd,ScreenType screenType);
    std::shared_ptr<SafeQueue<ScreenCommand>> normalQueue(){return normalQueue_;}
    std::shared_ptr<SafeQueue<ScreenCommand>> upgradeQueue(){return upgradeQueue_;}
    
private:
    std::shared_ptr<SafeQueue<ScreenCommand>> normalQueue_ ;
    std::shared_ptr<SafeQueue<ScreenCommand>> upgradeQueue_ ;

};
#endif
