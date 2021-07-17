#include <SFML/Graphics.hpp>

#include <iostream>
#include <sstream>

namespace color {

static const auto LIGHT_GREY = sf::Color(0xD3D3D3FF);
static const auto PEACH_PUFF = sf::Color(0xFFDAB9FF);
static const auto WHITE_SMOKE = sf::Color(0xF5F5F5FF);
static const auto LIGHT_DIM_GREY = sf::Color(0xC0C0C0FF);
static const auto DIM_GREY = sf::Color(0x696969FF);
static const auto GREY = sf::Color(0x808080FF);
static const auto SOFT_CYAN = sf::Color(0xB2F3F3FF);
static const auto ULTRA_RED = sf::Color(0xFC6C84FF);
static const auto BABY_BLUE = sf::Color(0x82D1F1FF);
static const auto RAINBOW_INDIGO = sf::Color(0x1e3f66FF);
static const auto SOFT_SEA_FOAM = sf::Color(0xDDFFEFFF);
static const auto SOFT_YELLOW = sf::Color(0xFFFFBFFF);

static const auto AVAILABLE_MOVE = SOFT_SEA_FOAM;

}  // namespace color

static constexpr float PIECE_RADIUS = 30;
static constexpr float CELL_SIZE = 80;
static const auto UNDEFINED_POSITION = sf::Vector2f{-100, -100};

template <class C>
class Enumerate {
public:
    explicit Enumerate(C& c, size_t index = 0) : c_(c), index_(index) {
    }

    Enumerate& operator++() {
        ++index_;
        return *this;
    }

    auto operator*() {
        return std::tie(index_, c_.at(index_));
    }

    bool operator!=(const Enumerate& rhs) const {
        return index_ != rhs.index_;
    }

    auto begin() { return Enumerate(c_); }

    auto end() { return Enumerate(c_, c_.size()); }

private:
    C& c_;
    size_t index_ = 0;
};
#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

class Task {
public:
    explicit Task(std::function<void()> function) : function_(std::move(function)) {
    }

    void operator()();

    void Cancel();

    bool IsCompleted() const;

    bool IsCompletedOrThrow() const;

    void Wait();

private:
    void ThrowIfError() const;

    bool completed_ = false;
    std::exception_ptr exceptionPtr_;
    std::function<void()> function_;

    std::condition_variable cv_;
    mutable std::mutex mutex_;
};

class ThreadPool {
public:
    explicit ThreadPool(size_t threadsNumber);

    ~ThreadPool();

    std::shared_ptr<Task> AddTask(std::function<void()> task);

    void Kill();

    void WaitAll();

private:
    void PollTasks();

    void Shutdown();

    std::condition_variable clientCv_;
    std::condition_variable cv_;
    std::mutex globalMutex_;
    bool shutdown_ = false;

    size_t inProcess_ = 0;
    std::deque<std::shared_ptr<Task>> tasks_;
    std::vector<std::thread> threads_;
};

void Task::operator()() {
    std::unique_lock lock(mutex_);
    if (!completed_) {  // TODO: canceled_
        try {
            function_();
            completed_ = true;
        } catch (...) {
            exceptionPtr_ = std::current_exception();
        }
    }
    cv_.notify_all();
}

void Task::Cancel() {
    std::unique_lock lock(mutex_);
    completed_ = true;  // TODO: canceled_
}

bool Task::IsCompleted() const {
    std::unique_lock lock(mutex_);
    return completed_;
}

bool Task::IsCompletedOrThrow() const {
    std::unique_lock lock(mutex_);
    if (completed_) {
        return true;
    }
    ThrowIfError();
    return false;
}

void Task::ThrowIfError() const {
    if (exceptionPtr_) {
        std::rethrow_exception(exceptionPtr_);
    }
}

void Task::Wait() {
    std::unique_lock lock(mutex_);
    while (!completed_ && exceptionPtr_ == nullptr) {
        cv_.wait(lock);
    }
}

ThreadPool::ThreadPool(size_t threadsNumber) {
    for (size_t i = 0; i < threadsNumber; ++i) {
        threads_.emplace_back([this]() {
            PollTasks();
        });
    }
}

ThreadPool::~ThreadPool() {
    Shutdown();
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

std::shared_ptr<Task> ThreadPool::AddTask(std::function<void()> task) {
    std::unique_lock lock(globalMutex_);
    if (shutdown_) {
        throw std::runtime_error("ThreadPool is shutting down.");
    }
    tasks_.emplace_back(std::make_shared<Task>(std::move(task)));
    cv_.notify_one();
    return tasks_.back();
}

void ThreadPool::Kill() {
    {
        std::unique_lock lock(globalMutex_);
        tasks_.clear();
    }
    Shutdown();
}

void ThreadPool::PollTasks() {
    std::unique_lock lock(globalMutex_);
    while (!shutdown_ || !tasks_.empty()) {
        if (tasks_.empty()) {
            clientCv_.notify_all();
            cv_.wait(lock);
        }

        while (!tasks_.empty()) {
            auto task = tasks_.front();
            tasks_.pop_front();
            ++inProcess_;

            lock.unlock();
            (*task)();
            lock.lock();

            --inProcess_;
        }

        if (inProcess_ == 0 && tasks_.empty()) {
            clientCv_.notify_all();
        }
    }
}

void ThreadPool::Shutdown() {
    std::unique_lock lock(globalMutex_);
    shutdown_ = true;
    cv_.notify_all();
}

void ThreadPool::WaitAll() {
    std::unique_lock lock(globalMutex_);

    while (inProcess_ > 0 || !tasks_.empty()) {
        clientCv_.wait(lock);
    }
}

class Logger {
public:
    explicit Logger(int id = 0) : id_(id) {
    }
    int id_;
};

class LineLogger {
public:
    explicit LineLogger(Logger& logger) : logger_(logger) {
        ss << logger_.id_ << ": ";
    }

    LineLogger(LineLogger&& rhs) noexcept : logger_(rhs.logger_), ss(std::move(rhs.ss)) {}

    ~LineLogger() {
        if (ss.rdbuf()->in_avail() == 0) {
            return;
        }
        ss << '\n';
        std::cerr << ss.str();
        std::cerr.flush();
    }

    std::stringstream ss;

private:
    Logger& logger_;
};

template <class T>
LineLogger&& operator<<(LineLogger&& logger, const T& value) {
    logger.ss << value;
    return std::move(logger);
}

template <class T>
LineLogger operator<<(Logger& logger, const T& value) {
    return std::move(std::move(LineLogger(logger)) << value);
}

Logger& Log() {
    thread_local Logger logger;
    return logger;
}
