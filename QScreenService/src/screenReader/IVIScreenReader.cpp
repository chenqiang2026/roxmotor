#include "screenReader/IVIScreenReader.h"
#include "utils/StringUtil.h"
IVIScreenReader::IVIScreenReader() 
    : ScreenReader() {
}

IVIScreenReader::~IVIScreenReader() {
}

int IVIScreenReader::readData(int curOpenScreenFd, char* buf, int len) {
    LOG_D("devScreenIviFd has data, reading...");
    int iviRead = ::read(curOpenScreenFd, buf, len);
       
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
            bool isUpdateCommand = (iviData.substr(0, strlen(lcdcommandBin)) == lcdcommandBin) || 
                                  (iviData.substr(0, strlen(lcdcommandTpbin)) == lcdcommandTpbin);
            if(isUpdateCommand){
                std::lock_guard<std::mutex> lock(upgradeMutex_);
                upgradeQueue_.Push(ScreenCommand{ScreenType::Ivi, iviData});
                upgradeCv_.notify_one();
            } else {
                safeQueue_.Push(ScreenCommand{ScreenType::Ivi, iviData});
            }
        }
    } else if (iviRead == -1) {
        ::close(curOpenScreenFd);
        curOpenScreenFd = -1;
        perror("read devScreenIviFd failed,%s %s",__LINE__,__func__);
    }
    
    return iviRead;
}
