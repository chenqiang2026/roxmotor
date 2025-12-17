#include <cerrno>
#include <memory>
#include <string>
#include <fcntl.h>
#include <cstring>  
#include <iostream>
#include <atomic>
#include <csignal>
#include <thread>
#include <unistd.h>
#include "../inc/I2cProcessHandler.h"
#include "../inc/core/ScreenDevPoller.h"
#include  "../inc/core/ScreenCommand.h"
#include "../inc/ScreenDeviceController.h"
#include "../inc/core/CmdExecutor.h"
#include "../inc/core/UpgradeCmdExecutor.h"
#include "../inc/Config.h"
#include "../inc/core/CommandDispatcher.h"


static std::atomic<bool> g_running{true};
static int openScreenDev(const std::string path) {
    int fd = ::open(path.c_str(), O_RDWR|O_NONBLOCK);
    if(fd < 0) {
        std::cerr << "open ("<< path <<") failed" << std::strerror(errno)<< std::endl;
        return -1;
    }
    return fd;
}
void sigint_handler(int) {
    g_running.store(false);
}
int main(int argc,char* argv[])
{
    std::signal(SIGINT, sigint_handler);
    static int clusterFd = -1,iviFd = -1,ceilingFd = -1;
    //打开设备
    clusterFd= openScreenDev(Config::screenDevCluster());
    iviFd= openScreenDev(Config::screenDevIVI());
    ceilingFd= openScreenDev(Config::screenDevCeiling());

    ScreenDevPoller poller;
    if(clusterFd > -1) poller.addFd(clusterFd);
    if(iviFd > -1) poller.addFd(iviFd);
    if(ceilingFd > -1) poller.addFd(ceilingFd);

    auto dispatcher = std::make_shared<CommandDispatcher>();
    poller.start([dispatcher](int fd, const std::string& cmd){
        ScreenType screenType = ScreenType::Unknown;
        if(fd == clusterFd) screenType = ScreenType::Cluster;
        else if(fd == iviFd) screenType = ScreenType::IVI;
        else if(fd == ceilingFd) screenType = ScreenType::Ceiling;
        dispatcher->dispatch(fd,cmd,screenType);
    });
    auto controller = std::make_shared<ScreenDeviceController>();
    auto normalExecutor = std::make_unique<CmdExecutor>(controller);
    normalExecutor->start(dispatcher->normalQueue());

    auto upgradeProcess = std::make_shared<UpgradeProcess>();
    auto upgradeExecutor = std::make_unique<UpgradeCmdExecutor>(upgradeProcess);
    upgradeExecutor->start(dispatcher->upgradeQueue());

    // 6) main loop waits for signal
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "Shutting down...\n";

    poller.stop();
    normalExecutor->stop();
    upgradeExecutor->stop();

    // close device fds
    if (clusterFd >= 0) ::close(clusterFd);
    if (iviFd >= 0)     ::close(iviFd);
    if (ceilingFd >= 0)    ::close(ceilingFd);
    std::cout << "Stopped\n";
    return 0;
}
