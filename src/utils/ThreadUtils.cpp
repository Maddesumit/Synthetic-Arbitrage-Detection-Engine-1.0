#include "ThreadUtils.hpp"
#include "Logger.hpp"
#include <future>

namespace arbitrage {
namespace utils {

// ThreadPool implementation
ThreadPool::ThreadPool(size_t num_threads) {
    threads_.reserve(num_threads);
    
    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back(&ThreadPool::workerLoop, this);
    }
    
    LOG_DEBUG("ThreadPool initialized with {} threads", num_threads);
}

ThreadPool::~ThreadPool() {
    should_stop_ = true;
    task_queue_.stop();
    
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    LOG_DEBUG("ThreadPool destroyed");
}

void ThreadPool::submit(Task task) {
    if (!should_stop_) {
        task_queue_.push(std::move(task));
    }
}



void ThreadPool::workerLoop() {
    Task task;
    while (!should_stop_) {
        if (task_queue_.pop(task)) {
            try {
                task();
            } catch (const std::exception& e) {
                LOG_ERROR("Exception in thread pool task: {}", e.what());
            } catch (...) {
                LOG_ERROR("Unknown exception in thread pool task");
            }
        }
    }
}

} // namespace utils
} // namespace arbitrage
