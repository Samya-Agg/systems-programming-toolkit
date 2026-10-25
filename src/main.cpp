#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <numeric>
#include <queue>
#include <thread>
#include <vector>

#include "thread_pool.hpp"
#include <future>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using Clock = std::chrono::steady_clock;

// Demonstrates the POSIX parent/child process lifecycle.
void run_process_demo() {
    std::cout << "\n[1] Process lifecycle demo\n";
    const pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        std::cout << "Child process: PID=" << getpid()
                  << ", parent PID=" << getppid() << std::endl;
        _exit(0);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return;
    }

    if (WIFEXITED(status)) {
        std::cout << "Parent process: child " << pid
                  << " exited with status " << WEXITSTATUS(status) << '\n';
    } else {
        std::cout << "Parent process: child ended abnormally\n";
    }
}

class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}

    void push(int value) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] { return queue_.size() < capacity_; });
        queue_.push(value);
        not_empty_.notify_one();
    }

    int pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return !queue_.empty(); });
        const int value = queue_.front();
        queue_.pop();
        not_full_.notify_one();
        return value;
    }

private:
    std::size_t capacity_;
    std::queue<int> queue_;
    std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
};

void run_producer_consumer_demo() {
    std::cout << "\n[2] Producer-consumer demo\n";
    constexpr int item_count = 20;
    BoundedQueue queue(4);
    long long sum = 0;

    std::thread producer([&queue] {
        for (int i = 1; i <= item_count; ++i) {
            queue.push(i);
        }
        queue.push(-1); // Sentinel signals the consumer to stop.
    });

    std::thread consumer([&queue, &sum] {
        while (true) {
            const int value = queue.pop();
            if (value == -1) break;
            sum += value;
        }
    });

    producer.join();
    consumer.join();

    const int expected = item_count * (item_count + 1) / 2;
    std::cout << "Consumed " << item_count << " items; sum=" << sum
              << " (expected " << expected << ")\n";
    std::cout << "Queue check: " << (sum == expected ? "PASS" : "FAIL") << '\n';
}

std::uint64_t compute_range(std::uint64_t begin, std::uint64_t end) {
    std::uint64_t total = 0;
    for (std::uint64_t i = begin; i < end; ++i) {
        total += (i % 97) * (i % 89);
    }
    return total;
}

struct Measurement {
    long long milliseconds;
    std::uint64_t result;
};

Measurement run_sequential(std::uint64_t work_items) {
    const auto start = Clock::now();
    const auto result = compute_range(0, work_items);
    const auto end = Clock::now();

    return {
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(),
        result
    };
}

Measurement run_parallel(std::uint64_t work_items, unsigned worker_count) {
    std::vector<std::uint64_t> partial(worker_count, 0);
    std::vector<std::thread> threads;
    threads.reserve(worker_count);

    const auto start = Clock::now();

    for (unsigned worker = 0; worker < worker_count; ++worker) {
        const std::uint64_t begin = work_items * worker / worker_count;
        const std::uint64_t end = work_items * (worker + 1) / worker_count;

        threads.emplace_back([&, worker, begin, end] {
            partial[worker] = compute_range(begin, end);
        });
    }

    for (auto& thread : threads) thread.join();

    const auto end = Clock::now();
    const auto result =
        std::accumulate(partial.begin(), partial.end(), std::uint64_t{0});

    return {
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(),
        result
    };
}

long long median(std::vector<long long> values) {
    std::sort(values.begin(), values.end());
    const std::size_t n = values.size();
    if (n % 2 == 1) return values[n / 2];
    return (values[n / 2 - 1] + values[n / 2]) / 2;
}

void run_benchmark_suite() {
    std::cout << "\n[3] Repeated sequential vs parallel benchmark\n";

    constexpr std::uint64_t kWorkItems = 80'000'000ULL;
    constexpr int kRepetitions = 5;
    const std::vector<unsigned> worker_counts = {1, 2, 4, 8};

    std::vector<long long> sequential_times;
    std::uint64_t expected_result = 0;
    sequential_times.reserve(kRepetitions);

    for (int run = 0; run < kRepetitions; ++run) {
        const Measurement measurement = run_sequential(kWorkItems);
        sequential_times.push_back(measurement.milliseconds);
        expected_result = measurement.result;
    }

    const long long sequential_median = median(sequential_times);
    std::cout << "Work items: " << kWorkItems
              << " | repetitions/configuration: " << kRepetitions << '\n';
    std::cout << "Sequential run times (ms): ";
    for (const auto value : sequential_times) std::cout << value << ' ';
    std::cout << "\nSequential median: " << sequential_median << " ms\n\n";

    std::cout << "Workers | run times (ms)                 | median (ms) | speedup | correctness\n";
    std::cout << "--------|---------------------------------|-------------|---------|------------\n";

    for (const unsigned workers : worker_counts) {
        std::vector<long long> times;
        bool correct = true;
        times.reserve(kRepetitions);

        for (int run = 0; run < kRepetitions; ++run) {
            const Measurement measurement = run_parallel(kWorkItems, workers);
            times.push_back(measurement.milliseconds);
            correct = correct && (measurement.result == expected_result);
        }

        const long long parallel_median = median(times);
        const double speedup = parallel_median > 0
            ? static_cast<double>(sequential_median) / parallel_median
            : 0.0;

        std::cout << workers << "       | ";
        for (const auto value : times) std::cout << value << ' ';
        if (times.size() < 5) std::cout << " ";
        std::cout << "| " << parallel_median
                  << "           | " << speedup << "x"
                  << "    | " << (correct ? "PASS" : "FAIL") << '\n';
    }

    std::cout << "\nNote: timings vary with hardware, compiler, power state, and system load.\n";
}


void run_thread_pool_demo() {
    std::cout << "\n[4] Reusable thread pool demo\n";

    ThreadPool pool(4);
    std::vector<std::future<int>> results;

    for (int i = 1; i <= 8; ++i) {
        results.push_back(
            pool.submit([i]() {
                return i * i;
            })
        );
    }

    int total = 0;

    for (auto& result : results) {
        total += result.get();
    }

    std::cout << "Tasks completed: " << results.size() << '\n';
    std::cout << "Sum of squares: " << total
              << " (expected 204)\n";
    std::cout << "Thread pool check: "
              << (total == 204 ? "PASS" : "FAIL") << '\n';
}


int main() {
    std::cout << "Systems Programming Toolkit (macOS/Linux)\n";
    run_process_demo();
    run_producer_consumer_demo();
    run_benchmark_suite();
    run_thread_pool_demo();
    return 0;
}
