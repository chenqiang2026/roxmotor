#include "screenReader/CeilingScreenReader.h"
#include "utils/StringUtil.h"

CeilingScreenReader::CeilingScreenReader() 
    : ScreenReader() {
}

CeilingScreenReader::~CeilingScreenReader() {
}

int CeilingScreenReader::readData(int curOpenScreenFd, char* buf, int len) {
    LOG_D("devScreenCeilingFd has data, reading...");
    int ceilingRead = ::read(curOpenScreenFd, buf, len);
    
    if (ceilingRead > 0) {
        std::string ceilingData(buf, ceilingRead);
        StringUtil::formatLcdcommand(ceilingData);
        bool isCommandMatched = false;  
        auto it = std::find_if(lcdCommandList_.begin(), lcdCommandList_.end(),
            [&ceilingData](const std::string& cmd) {
                return ceilingData.size() >= cmd.size() &&  
                    ceilingData.compare(0, cmd.size(), cmd) == 0;
            });
        isCommandMatched = (it != lcdCommandList_.end());
        
        if(isCommandMatched){
            bool isUpdateCommand = (ceilingData.substr(0, strlen(lcdcommandBin)) == lcdcommandBin) || 
                                  (ceilingData.substr(0, strlen(lcdcommandTpbin)) == lcdcommandTpbin);
            if(isUpdateCommand){
                std::lock_guard<std::mutex> lock(upgradeMutex_);
                upgradeQueue_.Push(ScreenCommand{ScreenType::Ceiling, ceilingData});
                upgradeCv_.notify_one();
            } else {
                safeQueue_.Push(ScreenCommand{ScreenType::Ceiling, ceilingData});
            }
        }
    } else if (ceilingRead == -1) {
        ::close(curOpenScreenFd);
        curOpenScreenFd = -1;
        perror("read devScreenCeilingFd failed, %s %s",__LINE__,__func__);
    }
    
    return ceilingRead;
}
