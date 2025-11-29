#include "CommandProcessor.h"
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>

CommandProcessor::~CommandProcessor() {
    stop();
}

void CommandProcessor::start() {
    isProcessing_ = true;
    processThread_ = std::thread(&CommandProcessor::popQueueCmd, this);
}

void CommandProcessor::stop() {
    isProcessing_ = false;
    if (processThread_.joinable()) {
        processThread_.join();
    }
    safeQueue_.stop();
    upgradeQueue_.stop();
}

void CommandProcessor::enqueueCommand(const DeviceCommand& cmd) {
    if (cmd.isUpgrade) {
        upgradeQueue_.push(cmd); 
    } else {
        safeQueue_.push(cmd); 
    }
}

void CommandProcessor::popQueueCmd() {
    while (isProcessing_) {
        DeviceCommand cmd;
        // 优先处理升级队列
        if (upgradeQueue_.pop(cmd, false)) {
            executeCommand(cmd);
        }
        // 再处理安全队列
        else if (safeQueue_.pop(cmd, false)) {
            executeCommand(cmd);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void CommandProcessor::executeCommand(const DeviceCommand& cmd) {
}
