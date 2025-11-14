#include "ServiceManager.h"
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <cstring>
#include <algorithm>
#include "Debug.h"
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "Main@QComService"
#endif

ServiceManager::ServiceManager() : fd_(-1) {}

ServiceManager::~ServiceManager() {
    shutDown();
}

void ServiceManager::shutDown() {
    std::lock_guard<std::mutex> lock(mutexSendChl_);
    for (auto client : clients_) {
        close(client);
    }
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
}

int ServiceManager::start() {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        LOGE("creat socket fail %s", strerror(errno));
        return -1;
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    auto state = bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (state != 0) {
        LOGE("bind socket fail %s", strerror(errno));
        close(fd_);
        return -1;
    }

    if (listen(fd_, QUEUE_SIZE) != 0) {
        LOGE("listen socket fail %s", strerror(errno));
        close(fd_);
        return -1;
    }

    fd_set rfds;
    int res, maxfd;
    FD_ZERO(&readfds_);
    FD_SET(fd_, &readfds_);
    maxfd = fd_;
    // LOGI("fd_ %d", fd_);
    while (fd_ > 0) {
        rfds = readfds_;
        res = select(maxfd+1, &rfds, nullptr, nullptr, nullptr);
        if (res < 0) {
            LOGI("select error=%s\n", strerror(errno));
            break;
        } else if (res == 0) {
            LOGI("select timeout=%s\n", strerror(errno));
        } else {
            if (FD_ISSET(fd_, &rfds)) {
                struct sockaddr_in remote = {};
                socklen_t len = sizeof(remote);
                int client = accept(fd_, (struct sockaddr*)&remote, &len);
                LOGI(" new client connect %d", client);
                if (client > 0) {
                    addSocket(client);
                    FD_SET(client, &readfds_);
                    if (client > maxfd) maxfd = client;
                }
                if (--res == 0) continue;
            }
            for (auto it = clients_.begin(); it != clients_.end();) {
                int client_socket = *it;
                it++;
                if (client_socket <= 0) continue;
                if (FD_ISSET(client_socket, &rfds)) {
                    handleClientData(client_socket);
                    if (--res == 0) break;
                }
            }
        }
    }
    return 0;
}

void ServiceManager::handleClientData(int client_socket) {
    // LOGI(" ready to read %d", client_socket);
    MsgTcpData recvMsg;
    int n = read(client_socket, reinterpret_cast<char*>(&recvMsg), sizeof(recvMsg));
    if (n <= 0) {
        // client disconnect
        LOGI("%d is disconnect", client_socket);
        handleClientDisconnect(client_socket);
    } else {
        // receive msg from client
        processData(recvMsg, client_socket);
    }
}

void ServiceManager::processData(const MsgTcpData& msg, int client_socket) {
    if (msg.msgid == 1000) {  // register
        LOGI("register: [name: %s, type: %d, domain: %d, port: %d]",
                msg.appName, msg.channelType, msg.domain, msg.port);
        ChannelInfo info;
        info.channelType = msg.channelType;
        info.domain = msg.domain;
        info.port = msg.port;
        info.socket = client_socket;
        snprintf(info.appName, sizeof(info.appName), "%s", msg.appName);
        registerChannel(info);
    }
}

void ServiceManager::handleClientDisconnect(int client_socket) {
    removeSocket(client_socket);
    FD_CLR(client_socket, &readfds_);
    unRegistChannel(client_socket);
}

bool ServiceManager::canBindtoRecv(const ChannelInfo& info) {
    std::lock_guard<std::mutex> lock(mutexSendChl_);
    for (auto recv : recvChannels_) {
        if (info.domain == recv.domain && info.port == recv.port) {
            return true;
        }
    }
    return false;
}

int ServiceManager::registerChannel(const ChannelInfo& info) {
    if (info.channelType == CHANNEL_TYPE_RECV) {
        auto state = addRecvChannel(info);
        broadcastRecvChannelOnline(info, state);
    } else if (info.channelType == CHANNEL_TYPE_SEND) {
        if (canBindtoRecv(info)) {
            auto ret = addSendChannel(info);
            notifySendChannelStatus(info, ret);
        } else {
            notifySendChannelStatus(info, -1);
        }
    } else {
        LOGE("channel type is set error");
    }
    return 0;
}

void ServiceManager::unRegistChannel(int socket) {
    LOGI(" socket %d", socket);
    std::vector<ChannelInfo> itemRecvs;
    std::vector<ChannelInfo> itemSends;
    {
        std::lock_guard<std::mutex> lock(mutexSendChl_);
        recvChannels_.erase(std::remove_if(recvChannels_.begin(), recvChannels_.end(),
                                            [&](const ChannelInfo& info) {
                                                if (info.socket == socket) {
                                                    itemRecvs.push_back(info);
                                                    return true;
                                                }
                                                return false;
                                            }),
                                recvChannels_.end());
    }
    {
        std::lock_guard<std::mutex> lock(mutexSendChl_);
        // for (auto item : sendChannels_) {
        //     LOGI(" ## [name %s, type %d, domain %d, port %d]",
        //      item.appName, item.channelType, item.domain, item.port);
        // }
        for (auto item : itemRecvs) {
            auto find_it = std::find_if(sendChannels_.begin(), sendChannels_.end(),
                    [&](const ChannelInfo& info) {
                        if (info.domain == item.domain && info.port == item.port) {
                            return true;
                        }
                        return false;
                    });
            if (find_it != sendChannels_.end()) {
                sendChannels_.erase(find_it);
            }
        }
        sendChannels_.erase(std::remove_if(sendChannels_.begin(), sendChannels_.end(),
                                            [&](const ChannelInfo& info) {
                                                if (info.socket == socket) {
                                                    itemSends.push_back(info);
                                                    return true;
                                                }
                                                return false;
                                            }),
                                sendChannels_.end());
        // for (auto item : sendChannels_) {
        //     LOGI(" after erase ## [name %s, type %d, domain %d, port %d]",
        //      item.appName, item.channelType, item.domain, item.port);
        // }
    }

    for (auto client : clients_) {
        // LOGI(" ready notify itemRecvs size %d", itemRecvs.size());
        // LOGI(" ready notify itemSends size %d", itemSends.size());
        for (auto info : itemRecvs) {
            notifyDisconnect(info, client);
        }
        for (auto info : itemSends) {
            notifyDisconnect(info, client);
        }
    }
}

/**
 * 不能添加重复的
 * @info want to add to sendChannels_
 * @:0 add success, -1: add fail
 */
int ServiceManager::addSendChannel(const ChannelInfo& info) {
    std::lock_guard<std::mutex> lock(mutexSendChl_);
    auto it = std::find_if(sendChannels_.begin(), sendChannels_.end(),
            [&](const ChannelInfo& item) {
                if ((strcmp(item.appName, info.appName) == 0)
                    && item.domain == info.domain
                    && item.port == info.port) {
                    LOGI(" %s has added ", info.appName);
                    return true;
                }
                return false;
            });
    if (it == sendChannels_.end()) {
        sendChannels_.push_back(info);
        return 0;
    }
    return -1;
}

/**
 * 不能添加名称重复的
 * @info want to add to recvChannels_
 * @:0 add success, -1: add fail
 */
int ServiceManager::addRecvChannel(const ChannelInfo& info) {
    std::lock_guard<std::mutex> lock(mutexRecvChl_);
    auto it = std::find_if(recvChannels_.begin(), recvChannels_.end(),
        [&](const ChannelInfo& item) {
            if (strcmp(info.appName, item.appName) == 0) {
                LOGI(" %s has added ", info.appName);
                return true;
            }
            return false;
        });
    if (it == recvChannels_.end()) {
        recvChannels_.push_back(info);
        return 0;
    }
    return -1;
}


void ServiceManager::addSocket(int socket) {
    std::lock_guard<std::mutex> lock(mutexAllSocket_);
    LOGI(" add socket %d", socket);
    clients_.push_back(socket);
}

void ServiceManager::removeSocket(int socket) {
    std::lock_guard<std::mutex> lock(mutexAllSocket_);
    clients_.erase(std::remove(clients_.begin(), clients_.end(), socket), clients_.end());
    close(socket);
}

void ServiceManager::broadcastRecvChannelOnline(const ChannelInfo& info, int state) {
    MsgTcpData msg;
    msg.msgid = 2000;
    msg.channelType = info.channelType;
    msg.domain = info.domain;
    msg.port = info.port;
    msg.state = state;
    snprintf(msg.appName, sizeof(msg.appName), "%s",  info.appName);
    LOGI("broadcast [%s, type(%d), domain(%d), port(%d), state(%d)] is online",
            msg.appName, msg.channelType, msg.domain, msg.port, state);
    std::lock_guard<std::mutex> lock(mutexAllSocket_);
    for (const auto& client : clients_) {
        int bytes_sent = send(client, reinterpret_cast<char*>(&msg), sizeof(msg), 0);
        if (bytes_sent == -1) {
            LOGE("send fail socket %d", client);
        }
    }
}

void ServiceManager::notifySendChannelStatus(const ChannelInfo& info, int state) {
    MsgTcpData msg;
    msg.msgid = 2001;
    msg.channelType = info.channelType;
    msg.domain = info.domain;
    msg.port = info.port;
    msg.state = state;
    snprintf(msg.appName, sizeof(msg.appName), "%s",  info.appName);
    LOGI("notify [%s, type(%d), domain(%d), port(%d)] register: %d",
            msg.appName, msg.channelType, msg.domain, msg.port, state);
    std::lock_guard<std::mutex> lock(mutexAllSocket_);
    for (const auto& client : clients_) {
        int bytes_sent = send(client, reinterpret_cast<char*>(&msg), sizeof(msg), 0);
        if (bytes_sent == -1) {
            LOGE("send fail socket %d", client);
        }
    }
}

void ServiceManager::notifyDisconnect(const ChannelInfo& info, int socket) {
    MsgTcpData msg;
    msg.msgid = 2002;
    msg.channelType = info.channelType;
    msg.domain = info.domain;
    msg.port = info.port;
    msg.state = 0;
    snprintf(msg.appName, sizeof(msg.appName), "%s",  info.appName);
    LOGI("notify [%s, type(%d), domain(%d), port(%d)] disconnet",
            msg.appName, msg.channelType, msg.domain, msg.port);
    int bytes_sent = send(socket, reinterpret_cast<char*>(&msg), sizeof(msg), 0);
    if (bytes_sent == -1) {
        LOGE("%s, send fail socket %d\n", __func__, socket);
    }
}

void ServiceManager::printRecv() {
    // LOGI("printRecv recvChannels_ size %d", (int)recvChannels_.size());
    // for (auto& item : recvChannels_) {
    //     LOGI("printRecv socket %d, appName %s, domain %d, port %d, send_chl_size %d",
    //      item.first, item.second.info.appName, item.second.info.domain,
    //       item.second.info.port, (int)item.second.sendChannels.size());
    // }
}
