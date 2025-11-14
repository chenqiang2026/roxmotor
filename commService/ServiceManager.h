#ifndef QCOMSERVICE_SERVICEMANAGER_H
#define QCOMSERVICE_SERVICEMANAGER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <map>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <mutex>  // NOLINT
#include <thread> // NOLINT
#include <utility>

typedef struct {
    int msgid;
    int channelType;  // send:0 recv:1
    int domain;  // android:1, qnx:2
    int port;
    int state;
    char appName[32];  // appname lenth < 32
} MsgTcpData;

typedef struct {
    int channelType;
    int domain;
    int port;
    int socket;
    char appName[32];
} ChannelInfo;

class ServiceManager {
 public:
    ServiceManager();
    ~ServiceManager();

    int start();

 private:
    void handleClientData(int client_socket);
    void processData(const MsgTcpData& msg, int client_socket);
    void broadcastRecvChannelOnline(const ChannelInfo& info, int state);
    void addSocket(int socket);
    void removeSocket(int socket);
    int registerChannel(const ChannelInfo& info);
    void handleClientDisconnect(int client_socket);
    int addSendChannel(const ChannelInfo& info);
    int addRecvChannel(const ChannelInfo& info);
    void unRegistChannel(int socket);
    void notifyDisconnect(const ChannelInfo& info, int socket);
    bool canBindtoRecv(const ChannelInfo& info);
    void notifySendChannelStatus(const ChannelInfo& info, int state);
    void printRecv();
    void shutDown();


 private:
    const char * SERVER_IP = "127.0.0.1";
    const int PORT = 10030;
    const int QUEUE_SIZE = 10;

    const int CHANNEL_TYPE_SEND = 0;
    const int CHANNEL_TYPE_RECV = 1;

 private:
    int Fd_;
    fd_set readfds_;
    std::vector<int> clients_;
    std::vector<ChannelInfo> sendChannels_;
    std::vector<ChannelInfo> recvChannels_;
    std::mutex mutexRecvChl_;
    std::mutex mutexAllSocket_;
    std::mutex mutexSendChl_;
};
#endif  // QCOMSERVICE_SERVICEMANAGER_H
