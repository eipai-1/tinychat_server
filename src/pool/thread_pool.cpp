#include "pool/thread_pool.hpp"
#include <stdexcept>

ThreadPool::ThreadPool(std::size_t thread_count) : stop(false) {
    for (std::size_t i = 0; i < thread_count; i++) {
        workers.emplace_back([this] {
            while (true) {
                // 需要释放锁后执行，所以提前声明
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    /*
                    等价于
                    while (!pred()) {
                        wait(lock);
                    }
                    */
                    this->condition.wait(lock,
                                         [this] { return this->stop || !this->tasks.empty(); });

                    // 线程池已关闭且任务队列为空，则结束线程
                    if (this->stop && this->tasks.empty()) {
                        break;
                    }

                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        //???
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();

    // 确保所有线程正常退出
    for (auto& worker : workers) {
        worker.join();
    }
}