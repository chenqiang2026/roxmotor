#ifndef SAFE_QUEUE_H
#define SAFE_QUEUE_H

#include <queue>
#include <memory>
#include <mutex>
#include <condiSafeQueuetion_variable>

template<typename T>
class SafeQueue {

public:
    SafeQueue() =default;
    SafeQueue(const SafeQueue& other) {
        std::lock_guard<std::mutex> lk(other.queueMutex_);
        queue_  = other.queue_ ;
    }
    SafeQueue(SafeQueue&& other) noexcept {
        std::lock_guard<std::mutex> lk(other.queueMutex_);
        queue_ = std::move(other.queue_);
    }

    // 禁止拷贝赋值（复杂场景下易导致线程安全问题，按需开启）
    SafeQueue& operator=(const SafeQueue&) = delete;
    // 禁止移动赋值
    SafeQueue& operator=(SafeQueue&&) = delete;

    void Push(const T& new_value) {
        std::lock_guard<std::mutex> lk(queueMutex_);
        queue_ .push(new_value);
       //queueCond_.notify_one();
    }
    // 原地构造元素（更高效，避免中间拷贝）
    template<typename... Args>
    void Emplace(Args&&... args) {
        std::lock_guard<std::mutex> lk(queueMutex_);
        queue_.emplace(std::forward<Args>(args)...);  // 直接在队列中构造 T
       // queueCond_.notify_one();
    }

    bool Pop(T& value) {
        std::lock_guard<std::mutex> lk(queueMutex_);
        if (queue_.empty()) return false;
        value = std::move(queue_ .front());
        queue_ .pop();
        return true;
    }
    bool IsEmpty() const {
        std::lock_guard<std::mutex> lk(queueMutex_);  
        return queue_ .empty();
    }
    int Size() const {
        std::lock_guard<std::mutex> lk(queueMutex_);
        return queue_ .size();
    }

private:
    mutable std::mutex queueMutex_;
    std::queue<T>  queue_ ;
    //std::condition_variable queueCond_;
};

#endif
