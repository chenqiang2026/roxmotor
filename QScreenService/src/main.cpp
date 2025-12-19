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
#include <sys/stat.h>
#include "../inc/I2cProcessHandler.h"
#include "../inc/core/ScreenDevPoller.h"
#include  "../inc/core/ScreenCommand.h"
#include "../inc/ScreenDeviceController.h"
#include "../inc/core/CmdExecutor.h"
#include "../inc/core/UpgradeCmdExecutor.h"
#include "../inc/Config.h"
#include "../inc/core/CommandDispatcher.h"


static std::atomic<bool> g_running{true};
 bool restartPpsService() 
 {
    std::cout << "==== Restarting PPS Service ====" << std::endl;
    //检查PPS服务是否正在运行
    int checkResult = system("pidin | grep -q '[p]ps'");
    if(WEXITSTATUS(checkResult) == 0) {
        std::cout << "PPS service is running, try to slay it" << std::endl;
        system("slay -f pps 2>/dev/null");
         (void)sleep(1);
    }
    // 清理旧的PPS目录
    struct stat st;
    if (stat("/tmp/pps", &st) == 0) {
        std::cout << "Removing existing /tmp/pps directory" << std::endl;
        if (system("rm -rf /tmp/pps") != 0) {
            std::cerr << "Failed to remove /tmp/pps directory" << std::endl;
            return false;
        }
    }
    //重启PPS服务
    int startResult = system("pps -m /tmp/pps -A /mnt/etc/pps_acl.conf -p /var/pps_persist -t 100");
    if (WEXITSTATUS(startResult) == 0) {
        std::cout << "PPS restarted successfully" << std::endl;
    } else {
        std::cerr << "Failed to restart PPS" << std::endl;
        return false;
    }
    //等待PPS服务完全启动
    (void)sleep(2);
    //检查PPS目录是否创建成功
    if (stat("/tmp/pps", &st) != 0) {
        if (errno == ENOENT) {
            std::cout << "PPS server isn't running." << std::endl;
            return false;
        } else {
            std::cerr << "stat (/tmp/pps) failed: " << std::strerror(errno) << std::endl;
            return false;
        }
    }
    std::cout << "==== PPS Service Restart Completed ====" << std::endl;
    return true;
}

static int openScreenDev(const std::string path) {
    int fd = ::open(path.c_str(), O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO );
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
    if (!restartPpsService()) {
        std::cerr << "Failed to restart PPS service. Exiting." << std::endl;
        return -1;
    }
    static int clusterFd = -1,iviFd = -1,ceilingFd = -1;
    //打开设备
    clusterFd= openScreenDev(Config::screenDevCluster());
    iviFd= openScreenDev(Config::screenDevIVI());
    ceilingFd= openScreenDev(Config::screenDevCeiling());

    ScreenDevPoller poller;
    if(clusterFd > -1) poller.addFd(clusterFd);
    if(iviFd > -1) poller.addFd(iviFd);
    if(ceilingFd > -1) poller.addFd(ceilingFd);

    //auto dispatcher = std::make_shared<CommandDispatcher>();
    auto dispatcher = new CommandDispatcher();
    poller.start([dispatcher](int fd, const std::string& cmd){
        ScreenType screenType = ScreenType::Unknown;
        if(fd == clusterFd) screenType = ScreenType::Cluster;
        else if(fd == iviFd) screenType = ScreenType::IVI;
        else if(fd == ceilingFd) screenType = ScreenType::Ceiling;
        dispatcher->dispatch(fd,cmd,screenType);
    });
    auto controller = new ScreenDeviceController();
    auto normalExecutor = new CmdExecutor(controller);
    normalExecutor->start(dispatcher->normalQueue());

    // auto upgradeProcess = std::make_shared<UpgradeProcess>();
    // auto upgradeExecutor = std::make_unique<UpgradeCmdExecutor>(upgradeProcess);
    // upgradeExecutor->start(dispatcher->upgradeQueue());

    // 6) main loop waits for signal
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "Shutting down...\n";
    
    poller.stop();
    normalExecutor->stop();
    delete dispatcher;
    delete controller;
    delete normalExecutor;
    //upgradeExecutor->stop();

    // close device fds
    if (clusterFd >= 0) ::close(clusterFd);
    if (iviFd >= 0)     ::close(iviFd);
    if (ceilingFd >= 0)    ::close(ceilingFd);
    std::cout << "Stopped\n";
    return 0;
}
