#include "../../inc/core/CommandDispatcher.h"
#include <cstring> 
#include <unistd.h> 
CommandDispatcher::CommandDispatcher():
normalQueue_(std::make_shared<SafeQueue<ScreenCommand>>()),
upgradeQueue_(std::make_shared<SafeQueue<ScreenCommand>>())
{
}
void CommandDispatcher::dispatch(int fd, const std::string& cmd,ScreenType screenType)
{
    ScreenCommand parsedCommand = ScreenCommand::parse(cmd,fd,screenType);
    if(!parsedCommand.valid){
        const char* err= "ERR:CommandDispatcher::dispatch invalid command \n";
        ::write(fd,err,strlen(err));
        return;
    }
    if(parsedCommand.isUpgrade){
        upgradeQueue_->Push(parsedCommand); 
    }else{
        normalQueue_->Push(parsedCommand);
    }
}
