#pragma once

#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>
#include <memory>
#include <functional>
#include <type_traits>
#include <future>

namespace arbitrage {
namespace utils {

/**
 * @brief Thread-safe queue implementation for high-performance producer-consumer patterns
 */
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;
    ~ThreadSafeQueue() = default;
    
    // Non-copyable
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
    
    /**
     * @brief Push item to queue
     * @param item Item to push
     */
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        condition_.notify_one();
    }
    
    /**
     * @brief Push item to queue (move semantics)
     * @param item Item to push
     */
    void push(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(item));
        condition_.notify_one();
    }
    
    /**
     * @brief Pop item from queue (blocking)
     * @param item Reference to store the popped item
     * @return true if item was popped, false if queue was stopped
     */
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !queue_.empty() || stopped_; });
        
        if (stopped_ && queue_.empty()) {
            return false;
        }
        
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    /**
     * @brief Try to pop item from queue (non-blocking)
     * @param item Reference to store the popped item
     * @return true if item was popped, false if queue is empty
     */
    bool tryPop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    /**
     * @brief Check if queue is empty
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    /**
     * @brief Get queue size
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    /**
     * @brief Stop the queue (unblocks all waiting threads)
     */
    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        condition_.notify_all();
    }

private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable condition_;
    std::atomic<bool> stopped_{false};
};

/**
 * @brief Thread pool for executing tasks asynchronously
 */
class ThreadPool {
public:
    using Task = std::function<void()>;
    
    /**
     * @brief Constructor
     * @param num_threads Number of worker threads
     */
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    
    /**
     * @brief Destructor - waits for all tasks to complete
     */
    ~ThreadPool();
    
    /**
     * @brief Submit a task for execution
     * @param task Task to execute
     */
    void submit(Task task);
    
    /**
     * @brief Submit a task and get a future for the result
     * @param func Function to execute
     * @param args Arguments for the function
     * @return Future for the result
     */
    template<typename F, typename... Args>
    auto submitWithResult(F&& func, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        using ReturnType = typename std::invoke_result<F, Args...>::type;
        
        auto task_ptr = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(func), std::forward<Args>(args)...)
        );
        
        std::future<ReturnType> result = task_ptr->get_future();
        
        Task wrapper = [task_ptr]() { (*task_ptr)(); };
        task_queue_.push(std::move(wrapper));
        
        return result;
    }
    
    /**
     * @brief Get number of worker threads
     */
    size_t getThreadCount() const { return threads_.size(); }
    
    /**
     * @brief Get number of pending tasks
     */
    size_t getPendingTasks() const { return task_queue_.size(); }

private:
    std::vector<std::thread> threads_;
    ThreadSafeQueue<Task> task_queue_;
    std::atomic<bool> should_stop_{false};
    
    void workerLoop();
};

/**
 * @brief RAII wrapper for read-write locks
 */
class ReadWriteLock {
public:
    ReadWriteLock() = default;
    
    /**
     * @brief Acquire shared (read) lock - simplified to use regular mutex for macOS compatibility
     */
    class ReadLock {
    public:
        explicit ReadLock(std::mutex& mutex) : lock_(mutex) {}
    private:
        std::unique_lock<std::mutex> lock_;
    };
    
    /**
     * @brief Acquire exclusive (write) lock
     */
    class WriteLock {
    public:
        explicit WriteLock(std::mutex& mutex) : lock_(mutex) {}
    private:
        std::unique_lock<std::mutex> lock_;
    };
    
    ReadLock acquireRead() { return ReadLock(mutex_); }
    WriteLock acquireWrite() { return WriteLock(mutex_); }

private:
    std::mutex mutex_;
};

/**
 * @brief High-resolution timer for performance measurements
 */
class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;
    
    /**
     * @brief Start timing
     */
    void start() {
        start_time_ = Clock::now();
    }
    
    /**
     * @brief Stop timing and return elapsed time in nanoseconds
     */
    long long stopNanoseconds() {
        auto end_time = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time - start_time_);
        return duration.count();
    }
    
    /**
     * @brief Stop timing and return elapsed time in microseconds
     */
    long long stopMicroseconds() {
        return stopNanoseconds() / 1000;
    }
    
    /**
     * @brief Stop timing and return elapsed time in milliseconds
     */
    long long stopMilliseconds() {
        return stopNanoseconds() / 1000000;
    }

private:
    TimePoint start_time_;
};

} // namespace utils
} // namespace arbitrage
