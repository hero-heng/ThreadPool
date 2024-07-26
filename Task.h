//
// Created by Henry Towne on 2024/7/23.
//

#ifndef THREADPOOL_TASK_H
#define THREADPOOL_TASK_H

#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>

// Any类型：可以接收任意数据的类型
class Any
{
public:
    Any() = default;
    ~Any() = default;
    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;
    Any(Any&&) = default;
    Any& operator=(Any&&) = default;

    // 这个构造函数可以让Any类型接收任意其他的数据
    template<typename T>
    Any(T data) : base_(std::make_unique<Derive<T>>(data)) {}

    // 把Any对象里面存储的data数据提取出来
    template<typename T>
    T cast_() {
        // 从base_找到它所指向的Derive对象，从它里面取出data成员变量
        Derive<T> *pd = dynamic_cast<Derive<T>*>(base_.get());
        if(pd == nullptr)
        {
            throw "type is unmatch!";
        }
        return pd->data_;
    }
private:
    // 基类类型
    class Base {
    public:
        virtual ~Base() = default;
    };

    // 派生类类型
    template<typename T>
    class Derive : public Base {
    public:
        Derive(T data) : data_(data) {}

        T data_;
    };

    // 定义一个基类的指针（基类的指针可以指向派生类对象）
    std::unique_ptr<Base> base_;
};

// 实现一个信号量类
class Semaphore {
public:
    Semaphore(int limit = 0)
    : resLimit_(limit)
    , isExit_(false)
    {}
    ~Semaphore()
    {
        isExit_ = true;
    }

    // 获取一个信号量资源
    void wait() {
        if(isExit_) return;

        std::unique_lock<std::mutex> lock(mtx_);
        // 等待信号量有资源，没有资源的话，会阻塞当前线程
        cond_.wait(lock,[&](){
            return resLimit_ > 0;
        });
        resLimit_--;
    }

    // 增加一个信号量资源
    void post() {
        if(isExit_) return;

        std::unique_lock<std::mutex> lock(mtx_);
        resLimit_++;
        cond_.notify_all();
    }
private:
    std::atomic_bool isExit_;
    int resLimit_;
    std::mutex mtx_;
    std::condition_variable cond_;
};

class Task;
// 实现接收提交到线程池的task任务执行完后的返回值类型Result
class Result {
public:
    Result(std::shared_ptr<Task> task, bool isValid = true);
    ~Result() = default;

    // 获取任务执行完的返回值
    void setVal(Any any);

    // get方法，用户调用这个方法获取task的返回值
    Any get();
private:
    Any any_; // 存储任务的返回值
    Semaphore sem_; // 线程通信信号量
    std::shared_ptr<Task> task_; // 指向对应获取返回值的任务对象
    std::atomic_bool isValid_; // 返回值是否有效
};

// 任务抽象基类
class Task {
public:
    Task() : result_(nullptr) {}
    ~Task() = default;

    void exec();
    void setResult(Result* res);
    // 用户可以自定义任意任务类型，从Task继承，重写run方法，实现自定义任务处理
    virtual Any run() = 0;
private:
    Result* result_;
};

#endif //THREADPOOL_TASK_H
