//
// Created by Henry Towne on 2024/7/23.
//
#ifndef THREADPOOL_THREADPOOL_H
#define THREADPOOL_THREADPOOL_H

#include "thread.h"
#include "Task.h"

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <unordered_map>

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
    Result submitTask(const std::shared_ptr<Task>& spTask);

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

    std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
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
