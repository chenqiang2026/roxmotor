#include "../../inc/core/ScreenDevPoller.h"
#include <poll.h>
#include <sched.h>
#include <thread>
#include <unistd.h>
#include <cstring>

ScreenDevPoller::ScreenDevPoller()
{
}
ScreenDevPoller::~ScreenDevPoller()
{
    stop();
}

void ScreenDevPoller::addFd(int fd)
{
    if(fd>0){
        pollFds_.push_back(fd);
    }
}

void ScreenDevPoller::start(Callback cb)
{
    if(running_.load()) return;
    running_.store(true);
    cb_ = std::move(cb);
    pollThread_ = std::thread(&ScreenDevPoller::pollReadFdLoop, this);
}

void ScreenDevPoller::pollReadFdLoop()
{
    std::vector<pollfd> pollFds(pollFds_.size());
    for(int i=0;i<pollFds_.size();i++){
        pollFds[i].fd = pollFds_[i];
        pollFds[i].events = POLLIN;
        pollFds[i].revents = 0;
    }

    while (true)
    {
        int ret = poll(pollFds.data(), pollFds.size(), -1);
        if (ret < 0) {
            if(errno == EINTR) continue;
            perror("poll");
            break;
        }
        if(ret == 0) continue;
        for(int i=0;i<pollFds.size();i++){
            if(pollFds[i].revents & POLLIN) {
                char buf[128];
                ssize_t readCount =::read(pollFds[i].fd,buf, sizeof(buf)-1);
                if(readCount > 0){
                    std::string readStr(buf,static_cast<size_t>(readCount));
                    while(!readStr.empty() && (readStr.back()=='\n'|| readStr.back()=='\r')) {
                        readStr.pop_back();
                    }
                    if(!readStr.empty() && cb_){
                        cb_(pollFds[i].fd,readStr);
                    } 

                } else if(readCount == 0){
                    //LOD_D("readCount == 0, fd: %d", pollFds[i].fd);
                } else {
                    //LOD_E("read error, fd: %d, errno: %s", pollFds[i].fd, strerror(errno));
                }
            }
        }
    }
}

void ScreenDevPoller::stop()
{
    if(!running_) return;
    running_.store(false);
    if(pollThread_.joinable()){
        pollThread_.join();
    }
}
