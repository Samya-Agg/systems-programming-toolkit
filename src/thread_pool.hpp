
#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <stdexcept>
#include <memory>

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <type_traits>
#include <utility>

class ThreadPool {
public:
    explicit ThreadPool(std::size_t worker_count);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename Function, typename... Args>
    auto submit(Function&& function, Args&&... args)
        -> std::future<std::invoke_result_t<Function, Args...>>
    {
        using Result = std::invoke_result_t<Function, Args...>;

        auto task = std::make_shared<std::packaged_task<Result()>>(
            [fn = std::forward<Function>(function),
             ... arguments = std::forward<Args>(args)]() mutable -> Result {
                return std::invoke(
                    std::move(fn),
                    std::move(arguments)...
                );
            }
        );

        std::future<Result> result = task->get_future();

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (stopping_) {
                throw std::runtime_error("ThreadPool is stopping");
            }

            tasks_.emplace([task]() {
                (*task)();
            });
        }

        condition_.notify_one();
        return result;
    }

private:
    void worker_loop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool stopping_ = false;
};

#endif
