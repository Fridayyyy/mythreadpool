//
// Created by 常贵杰 on 2022/9/7.
//

#ifndef MYTHREADPOOL_THREADPOOL_H
#define MYTHREADPOOL_THREADPOOL_H
#include <mutex>
#include <queue>
#include <future>
#include <thread>
#include <utility>
#include <vector>
#include <functional>

template<typename T>
class SafeQueue{
private:
    std::queue<T> m_queue;
    std::mutex m_mutex;

public:
    SafeQueue(){}
    SafeQueue(SafeQueue &&other){}
    ~SafeQueue(){}
    bool empty(){
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }
    int size(){
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue.size();
    }
    void enqueue(T &t){
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.template emplace(t);
    }
    bool dequeue(T &t){
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_queue.empty())
            return false;
        t=std::move(m_queue.front());
        m_queue.pop();
        return true;
    }
};

class ThreadPool {
private:
    class ThreadWorker{
    private:
        int m_id;

        ThreadPool *m_pool;//所属线程池
    public:
        ThreadWorker(ThreadPool *pool,const int id)
            :m_pool(pool),m_id(id){

        }

        void operator()(){
            std::function<void()> func;
            bool dequeued;//是否正在取出队列中的元素
            while (!m_pool->m_shutdown){
                {
                    std::unique_lock<std::mutex> lock(m_pool->m_conditional_mutex);
                    if (m_pool->m_queue.empty()){
                        //等待条件变量通知，开启线程
                        m_pool->m_condition_lock.wait(lock);
                    }
                    dequeued=m_pool->m_queue.dequeue(func);
                }
                if (dequeued)
                    func();
            }
        }
    };

    bool m_shutdown;
    SafeQueue<std::function<void()>> m_queue;
    std::vector<std::thread> m_threads;//工作线程队列
    std::mutex m_conditional_mutex;//线程休眠锁互斥变量
    std::condition_variable m_condition_lock;//环境变量锁
public:
    ThreadPool(const int n_threads=4)
        :m_threads(std::vector<std::thread>(n_threads)),m_shutdown(false){

    }
    ThreadPool(const ThreadPool &)=delete;
    ThreadPool(ThreadPool &&)=delete;
    ThreadPool &operator=(const ThreadWorker &)=delete;
    ThreadPool &operator=(ThreadPool &&)=delete;

    void init(){
        for (int i = 0; i < m_threads.size(); ++i) {
            m_threads.at(i)=std::thread(ThreadWorker(this,i));
        }
    }

    void shutdown(){
        m_shutdown= true;
        m_condition_lock.notify_all();
        for (int i = 0; i < m_threads.size(); ++i) {
            if (m_threads.at(i).joinable())
                m_threads.at(i).join();
        }
    }

    template <typename F,typename ...Args>
    auto submit(F &&f,Args &&...args)->std::future<decltype(f(args...))>{
        std::function<decltype(f(args...))()> func=std::bind(std::forward<F>(f),std::forward<Args>(args)...);
        auto task_ptr=std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

        std::function<void()> warpper_func=[task_ptr](){
            (*task_ptr)();
        };
        m_queue.enqueue(warpper_func);
        m_condition_lock.notify_one();

        return task_ptr->get_future();

    }
};


#endif //MYTHREADPOOL_THREADPOOL_H
