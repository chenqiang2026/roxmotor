#include "ClusterScreenReader.h"
#include "utils/StringUtil.h"
int ClusterScreenReader::readData(int curOpenScreenFd, char* buf, int len) {
    LOG_D("devScreenClusterFd has data, reading...");
    int clusterRead = ::read(curOpenScreenFd, buf, len);
    
    if (clusterRead > 0) {
        std::string clusterData(buf, clusterRead);
        StringUtil::formatLcdcommand(clusterData);
        
        // 集群屏幕特定的命令处理逻辑
        bool isCommandMatched = false;  
        auto it = std::find_if(lcdCommandList_.begin(), lcdCommandList_.end(),
            [&clusterData](const std::string& cmd) {
                return clusterData.size() >= cmd.size() &&  
                    clusterData.compare(0, cmd.size(), cmd) == 0;
            });
        isCommandMatched = (it != lcdCommandList_.end());
        
        if(isCommandMatched){
            bool isUpdateCommand = (clusterData.substr(0, strlen(lcdcommandBin)) == lcdcommandBin) || 
                                  (clusterData.substr(0, strlen(lcdcommandTpbin)) == lcdcommandTpbin);
            if(isUpdateCommand){
                std::lock_guard<std::mutex> lock(upgradeMutex_);
                upgradeQueue_.Push(ScreenCommand{ScreenType::Cluster, clusterData});
                upgradeCv_.notify_one();
            } else {
                safeQueue_.Push(ScreenCommand{ScreenType::Cluster, clusterData});
            }
        }
    } else if (clusterRead == -1) {
        ::close(curOpenScreenFd);
        curOpenScreenFd = -1;
        perror("read devScreenClusterFd failed");
    }
    
    return clusterRead;
}
