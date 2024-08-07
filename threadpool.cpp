//
// Created by Henry Towne on 2024/7/23.
//
#include "threadpool.h"

#include <functional>
#include <iostream>
#include <thread>

const int TASK_MAX_THRESHOLD = 1024;
const int THREAD_MAX_THRESHOLD = 100;
const int THREAD_MAX_IDLE_TIME = 60; //单位 s

ThreadPool::ThreadPool()
    : initThreadSize_(4)
    , taskSize_(0)
    , idleThreadSize_(0)
    , curThreadSize_(0)
    , threadSizeThreshold_(THREAD_MAX_THRESHOLD)
    , taskQueMaxThreshold_(TASK_MAX_THRESHOLD)
    , poolMode_(PoolMode::MODE_FIXED)
    ,isPoolRunning_(false)
{}

ThreadPool::~ThreadPool()
{
    isPoolRunning_ = false;

    // 等待线程池里面所有的线程返回 有两种状态：阻塞 & 正在执行任务中
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    notEmpty_.notify_all();
    exitCond_.wait(lock, [&](){
        return threads_.empty();
    });
}

// 给线程池提交任务 用户调用该接口，传入任务对象，生产任务
Result ThreadPool::submitTask(const std::shared_ptr<Task>& spTask)
{
    /**
     * 获取锁
     * 线程的通信 等待任务队列有空余
     * 如果有空余，把任务放入任务队列中
     * 因为新放了任务，任务队列肯定不空了，在notEmpty_上进行通知
     */
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    // 用户提交任务，最长不能阻塞超过1s，否则判断提交任务失败，返回
    if(!notFull_.wait_for(lock,std::chrono::seconds(1),[this](){
        return taskQue_.size() < (size_t)taskQueMaxThreshold_;
    })) { // 表示notFull等待1s，条件依然没有满足
        std::cerr << "task queue is full,submit task fail." << std::endl;
//        return task->getResult(); // 线程执行完task，task对象就被析构掉了
        return Result(spTask, false);
    }
    taskQue_.push(spTask);
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

    // return task->getResult();
    return Result(spTask);
}

//定义线程函数 线程池的所有线程从任务队列里面消费任务
void ThreadPool::threadFunc(int threadId) {
    /**
     * 先获取锁
     * 等待notEmpty条件
     * 从任务队列中取一个任务出来
     * 当前线程负责执行这个任务
     */
    auto lastTime = std::chrono::high_resolution_clock().now();
    for(;;) {
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lock(taskQueMtx_);

            std::cout<<"tid:"<<std::this_thread::get_id()
                << "尝试获取任务..."<<std::endl;

            // 锁 + 双重判断
            while (taskQue_.empty()) {
                // 线程池要结束，回收线程资源
                if(!isPoolRunning_)
                {
                    threads_.erase(threadId);
                    exitCond_.notify_all();
                    std::cout<<"threadid:"<<std::this_thread::get_id()<<" exit!"<<std::endl;
                    return;
                }
                // cached模式下，有可能已经创建了很多线程，但是空闲时间超过60s
                // 应该把多余的线程结束回收掉（超过initThreadSize_ 数量的线程要进行回收）
                if(poolMode_ == PoolMode::MODE_CACHED) {
                    // 每一秒中返回一次，区分：超时返回还是有任务待执行返回
                    if(std::cv_status::timeout == notEmpty_.wait_for(lock,std::chrono::seconds(1)))
                    {
                        auto nowTime = std::chrono::high_resolution_clock().now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(nowTime - lastTime);
                        if(dur.count() >= THREAD_MAX_IDLE_TIME
                           && curThreadSize_ > initThreadSize_) {
                            threads_.erase(threadId);
                            curThreadSize_--;
                            idleThreadSize_--;

                            std::cout<<"threadid:"<<std::this_thread::get_id()<<" exit!"<<std::endl;
                            return;
                        }
                    }
                }else {
                    notEmpty_.wait(lock);
                }

//                if(!isPoolRunning_) {
//                    threads_.erase(threadId);
//                    std::cout<<"threadid:"<<std::this_thread::get_id()<<" exit!"<<std::endl;
//                    exitCond_.notify_all();
//                    return;
//                }
            }

            if(!isPoolRunning_) {
                break;
            }

            idleThreadSize_--;

            std::cout<<"tid:"<<std::this_thread::get_id()
                     << "获取任务成功..."<<std::endl;

            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;

            // 如果依然有任务，继续通知其他线程执行任务
            if(!taskQue_.empty()) {
                notEmpty_.notify_all();
            }
            notFull_.notify_all();
        }// 取出任务后，执行任务前就应该把锁释放掉

        if(task != nullptr) {
//            task->run();
            task->exec();
        }

        idleThreadSize_++;
        lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间
    }
}

void ThreadPool::start()
{
    isPoolRunning_ = true;
    curThreadSize_ = initThreadSize_;

    // 创建线程对象
    for (int i = 0; i < initThreadSize_; ++i) {
        // 创建thread线程对象的时候，把线程函数给到thread线程对象
        auto thread_ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
        int thread_id = thread_ptr->getId();
        threads_.emplace(thread_id,std::move(thread_ptr));
//        threads_.emplace_back(std::move(thread_ptr));
    }

    //启动所有线程
    for (int i = 0; i < initThreadSize_; ++i) {
        threads_[i]->start();
        idleThreadSize_++; // 记录初始空闲线程的数量
    }
}

bool ThreadPool::checkRunningState() const {
    return isPoolRunning_;
}

void ThreadPool::setPoolMode(PoolMode poolMode) {
    if(!checkRunningState()){
        poolMode_ = poolMode;
    }
}

void ThreadPool::setThreadMaxThreshold(int threshold) {
    if(checkRunningState() || poolMode_ != PoolMode::MODE_CACHED) {
        return;
    }
    threadSizeThreshold_ = threshold;
}
