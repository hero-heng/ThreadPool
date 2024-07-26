//
// Created by Henry Towne on 2024/7/23.
//
#ifndef THREADPOOL_THREADPOOL_H
#define THREADPOOL_THREADPOOL_H

#include "thread.h"

#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <thread>
#include <future>

enum class PoolMode {
    MODE_FIXED, // 固定数量的线程池
    MODE_CACHED // 线程数量可动态增长
};

class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    // 设置线程池的工作模式
    void setPoolMode(PoolMode poolMode);

    void setTaskQueMaxThreshold(int threshold) { taskQueMaxThreshold_ = threshold; }

    void setThreadMaxThreshold(int threshold);

    void setInitThreadSize(int threadSize) { initThreadSize_ = threadSize;}

    // 给线程池提交任务
    // 使用可变参模板编程，让submitTask可以接收任意任务函数和任意数量的参数
    template<typename Func, typename... Args>
    auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
    {
        using Rtype = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<Rtype()>>(
                std::bind(std::forward<Func>(func),std::forward<Args>(args)...));
        std::future<Rtype> result = task->get_future();

        std::unique_lock<std::mutex> lock(taskQueMtx_);
        // 用户提交任务，最长不能阻塞超过1s，否则判断提交任务失败，返回
        if(!notFull_.wait_for(lock,std::chrono::seconds(1),[this](){
            return taskQue_.size() < (size_t)taskQueMaxThreshold_;
        })) { // 表示notFull等待1s，条件依然没有满足
            std::cerr << "task queue is full,submit task fail." << std::endl;
            auto taskTemp = std::make_shared<std::packaged_task<Rtype()>>(
                    []()->Rtype {return Rtype();});
            (*taskTemp)();
            return taskTemp->get_future();
        }
//    taskQue_.push(spTask);
        taskQue_.emplace([&](){
            (*task)();
        });
        taskSize_++;
        notEmpty_.notify_all();

        // cached模式 需要根据任务数量和空闲线程的数量，判断是否需要创建新的线程
        if(poolMode_ == PoolMode::MODE_CACHED
           && taskSize_ > idleThreadSize_
           && curThreadSize_ < threadSizeThreshold_)
        {
            std::cout<<"======create new thread....======="<<std::endl;

            auto thread_ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
            int thread_id = thread_ptr->getId();
            threads_.emplace(thread_id,std::move(thread_ptr));
            threads_[thread_id]->start(); // 启动线程
            curThreadSize_++;
            idleThreadSize_++;
        }

        return result;
    }

    // 开始线程池
    void start();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
private:
    // 定义线程函数
    void threadFunc(int threadId);

    bool checkRunningState() const;

//    std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
    std::unordered_map<int, std::unique_ptr<Thread>> threads_; // 线程列表

    int initThreadSize_; // 初始的线程数量
    std::atomic_int curThreadSize_; // 记录当前线程池里面线程的总数量
    int threadSizeThreshold_; // 线程数量阈值
    std::atomic_int idleThreadSize_; // 记录空闲线程的数量

    using Task = std::function<void()>;
    std::queue<Task> taskQue_; // 任务队列
    std::atomic_int taskSize_; // 任务的数量
    int taskQueMaxThreshold_; // 任务队列数量阈值

    std::mutex taskQueMtx_; // 保证任务队列的线程安全
    std::condition_variable notFull_; // 表示任务队列不满
    std::condition_variable notEmpty_; // 表示任务队列不空
    std::condition_variable exitCond_; // 等待线程资源全部回收

    PoolMode poolMode_; // 当前线程池的工作模式
    std::atomic_bool isPoolRunning_; // 表示当前线程池的运行状态
};

#endif //THREADPOOL_THREADPOOL_H
