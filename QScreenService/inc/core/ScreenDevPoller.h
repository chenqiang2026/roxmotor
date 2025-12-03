#ifndef SCREEN_DEV_POLLER_H
#define SCREEN_DEV_POLLER_H
#include <functional>
#include <sys/poll.h>
#include <thread>
#include <atomic>
class ScreenDevPoller
{
public:
    ScreenDevPoller();
    ~ScreenDevPoller();
    using Callback= std::function<void(const int fd , const std::string& cmd)>;
    void addFd(int fd);
    void start(Callback cb);
    void stop();
private:
    void pollReadFdLoop();
private:
    std::vector<int> pollFds_{};
    std::thread pollThread_;
    Callback cb_;
    std::atomic<bool> running_{false};

};
#endif
