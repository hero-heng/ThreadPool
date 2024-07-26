//
// Created by Henry Towne on 2024/7/23.
//
#include "threadpool.h"

#include <functional>
#include <iostream>
#include <thread>

const int TASK_MAX_THRESHOLD = 1024;
const int THREAD_MAX_THRESHOLD = 100;
const int THREAD_MAX_IDLE_TIME = 60; //��λ s

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

    // �ȴ��̳߳��������е��̷߳��� ������״̬������ & ����ִ��������
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    notEmpty_.notify_all();
    exitCond_.wait(lock, [&](){
        return threads_.empty();
    });
}

//�����̺߳��� �̳߳ص������̴߳��������������������
void ThreadPool::threadFunc(int threadId) {
    /**
     * �Ȼ�ȡ��
     * �ȴ�notEmpty����
     * �����������ȡһ���������
     * ��ǰ�̸߳���ִ���������
     */
    auto lastTime = std::chrono::high_resolution_clock().now();
    for(;;) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(taskQueMtx_);

            std::cout<<"tid:"<<std::this_thread::get_id()
                << "���Ի�ȡ����..."<<std::endl;

            // �� + ˫���ж�
            while (taskQue_.empty()) {
                // �̳߳�Ҫ�����������߳���Դ
                if(!isPoolRunning_)
                {
                    threads_.erase(threadId);
                    exitCond_.notify_all();
                    std::cout<<"threadid:"<<std::this_thread::get_id()<<" exit!"<<std::endl;
                    return;
                }
                // cachedģʽ�£��п����Ѿ������˺ܶ��̣߳����ǿ���ʱ�䳬��60s
                // Ӧ�ðѶ�����߳̽������յ�������initThreadSize_ �������߳�Ҫ���л��գ�
                if(poolMode_ == PoolMode::MODE_CACHED) {
                    // ÿһ���з���һ�Σ����֣���ʱ���ػ����������ִ�з���
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
                     << "��ȡ����ɹ�..."<<std::endl;

            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;

            // �����Ȼ�����񣬼���֪ͨ�����߳�ִ������
            if(!taskQue_.empty()) {
                notEmpty_.notify_all();
            }
            notFull_.notify_all();
        }// ȡ�������ִ������ǰ��Ӧ�ð����ͷŵ�

        if(task != nullptr) {
            task();
        }

        idleThreadSize_++;
        lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��
    }
}

void ThreadPool::start()
{
    isPoolRunning_ = true;
    curThreadSize_ = initThreadSize_;

    // �����̶߳���
    for (int i = 0; i < initThreadSize_; ++i) {
        // ����thread�̶߳����ʱ�򣬰��̺߳�������thread�̶߳���
        auto thread_ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
        int thread_id = thread_ptr->getId();
        threads_.emplace(thread_id,std::move(thread_ptr));
//        threads_.emplace_back(std::move(thread_ptr));
    }

    //���������߳�
    for (int i = 0; i < initThreadSize_; ++i) {
        threads_[i]->start();
        idleThreadSize_++; // ��¼��ʼ�����̵߳�����
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

