#pragma once

#include <vector>
#include <functional>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cstddef>
#include <future>
#include <iostream>

namespace tcs {
namespace pool {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t thread_count = std::thread::hardware_concurrency());

    /*
    根据调用的函数决定是否有返回值
    如果调用函数有返回值则addTask返回future包装类，如果没有返回值则addTask也没有返回值
    */
    template <typename F, typename... Args>
    auto addTask(F&& f, Args&&... args) {
        // 返回值类型
        using return_type = typename std::invoke_result<F, Args...>::type;

        // 如果没有返回值
        if constexpr (std::is_void_v<return_type>) {
#ifndef NDEBUG
            std::cout << "Calling addTask without return value" << std::endl;
#endif
            auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                if (stop) {
                    throw std::runtime_error("AddTask on a stopped ThreadPool");
                }
                tasks.emplace(std::move(task));
            }
            condition.notify_one();

            // 如果函数有返回值
        } else {
#ifndef NDEBUG
            std::cout << "Calling addTask with return value" << std::endl;
#endif
            // packaged_task只能移动，但是std::function要求可拷贝
            // 所以引入share_ptr
            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));

            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queue_mutex);

                // 线程池停止，不再添加任务
                if (stop) {
                    throw std::runtime_error("AddTask on a stopped ThreadPool");
                }

                //???
                tasks.emplace([task] { (*task)(); });
            }
            condition.notify_one();
            return res;
        }
    }

    /* todo: with callback */
    template <typename F, typename... Args, typename C>
    void addTaskCallback(F&& f, C&& on_compelete, Args&&... args) {
        // 返回值类型
        auto task_payload = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        auto wrapper_task = [payload = std::move(task_payload),
                             callback = std::forward<C>(on_compelete)] {
            auto result = payload();
            return callback(result);
        };

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            // 线程池停止，不再添加任务
            if (stop) {
                throw std::runtime_error("AddTask on a stopped ThreadPool");
            }
            tasks.emplace(std::move(wrapper_task));
        }
        condition.notify_one();
    }

    ~ThreadPool();

    std::size_t getThreadCount() { return workers.size(); }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};
}  // namespace pool
}  // namespace tcs