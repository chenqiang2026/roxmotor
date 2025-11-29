#ifndef DATA_READER_H
#define DATA_READER_H

#include "ScreenDeviceManager.h"
#include <queue>
#include <mutex>
#include <condition_variable>

class DataReader : public ScreenDeviceManager {
public:
    DataReader() = default;
    ~DataReader() { stop(); }

    // 启动/停止读取线程
    void start() {
        isReading_ = true;
        readThread_ = std::thread(&DataReader::readLoop, this);
    }

    void stop() {
        isReading_ = false;
        if (readThread_.joinable()) {
            readThread_.join();
        }
        dataQueue_.stop();
    }

    // 获取数据队列（供外部消费）
    SafeQueue<DeviceData>& getDataQueue() { return dataQueue_; }

private:
    // 读取循环：从设备FD读取数据并入队
    void readLoop() {
        while (isReading_) {
            for (const auto& [fd, type] : fdToScreenTypeMap_) {
                uint8_t buf[1024] = {0};
                ssize_t n = ::read(fd, buf, sizeof(buf));
                if (n > 0) {
                    dataQueue_.push({fd, type, {buf, buf + n}});
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 降低CPU占用
        }
    }

    std::atomic<bool> isReading_ = false;
    std::thread readThread_;
    SafeQueue<DeviceData> dataQueue_; // 数据队列
};

#endif // DATA_READER_H
