#ifndef SCREEN_READER_H
#define SCREEN_READER_H

#include <string>
#include <cstdint>
#include <vector>
#include "ScreenCommand.h"
#include <queue>
#include <mutex>
#include <condition_variable>

class ScreenReader {
public:
    ScreenReader( ) {}
    virtual ~ScreenReader() = default;
    
    virtual int readData(int curOpenScreenFd, char* buf, int len) = 0;

protected:
    std::vector<std::string> lcdCommandList_;
    Queue<ScreenCommand> upgradeQueue_;
    Queue<ScreenCommand> safeQueue_;
    std::mutex upgradeMutex_;
    std::condition_variable upgradeCv_;
    static constexpr const char* lcdcommandBin{"lcdupdate-p"};  
    static constexpr const char* lcdcommandTpbin{"lcdupdatetp-p"}; 
};

#endif // SCREEN_READER_H
