// threadpool with task as void function (don't return any result)

#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <future>

class ThreadPool
{
private:
    std::mutex task_vector_mtx;
    std::atomic_bool stop = false;
    std::condition_variable thread_queue_cv;

    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;

public:
    ThreadPool(size_t number_threads = 1)
    {
        // create N threads
        for (size_t i = 0; i < number_threads; i++)
        {
            // create thread_member
            std::thread thread_member([this]() {
                while (true)
                {
                    std::function<void()> task;
                    // thread wait for being assigned task 
                    // use mutex to lock when try to get task from list 
                    {
                        std::unique_lock<std::mutex> lock(task_vector_mtx);
                        thread_queue_cv.wait(lock, [this] {
                            return !tasks.empty() || stop;
                        }); 
                        
                        // return if threadpool is terminated actively or empty tasks 
                        if (stop && tasks.empty())
                        { 
                            return; 
                        }
                        
                        // get task from list tasks 
                        task = std::move(tasks.front());
                        tasks.pop();
                    } 
                    
                    // do task()
                    // std::cout << "do task" << std::endl;
                    task();
                    // std::cout << "do task completed" << std::endl;
                }
            });

            // store thread_member in pool
            threads.push_back(std::move(thread_member));
        }
    }

    // public API to add new task to ThreadPool
    void addTask(std::function<void()> &&task)
    {
        {
            std::unique_lock<std::mutex> lock(task_vector_mtx);
            tasks.push(task);
        }
        thread_queue_cv.notify_one();
    }

    ~ThreadPool()
    {
        std::cout << "ThreadPool deconstructor is called" << std::endl;
        {
            std::unique_lock<std::mutex> lock(task_vector_mtx);
            stop = true;
        }

        // notify all threads to wakeup and join to main thread
        // if thread completed, it will be terminated
        thread_queue_cv.notify_all();
        // thread completed work need to join (or detach)
        for (std::thread &thread : threads)
        {
            thread.join();
        }
    }
};

int task(int number)
{
    // std::cout << "Task " << number << " is running on thread " << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return number * 10;
}


class FuturePool {
private:
    std::mutex future_mtx;
    std::atomic_bool stop = false;
    std::condition_variable future_cv;
    std::queue<std::future<int>> future_queue;
    std::thread thread;
    public:
    FuturePool()
    {
        std::thread thread_member([this] 
        {
            while (true)
            {
                std::future<int> future;
                {
                    std::unique_lock<std::mutex> lock(future_mtx);
                    future_cv.wait(lock, [this] {
                        return !future_queue.empty() || stop;
                    });
                    if (stop && future_queue.empty())
                    {
                        return;
                    }
                    future = std::move(future_queue.front());
                    future_queue.pop();
                }
                std::cout << "return value = " << future.get() << std::endl;
            }
        });
        thread = std::move(thread_member);
    }

    void addFuture(std::future<int>&& future)
    {
        {
            std::unique_lock<std::mutex> lock(future_mtx);
            future_queue.push(std::move(future));
        }
        future_cv.notify_one();
    }

    ~FuturePool()
    {
        {
            std::unique_lock<std::mutex> lock(future_mtx);
            stop = true;
        }
        future_cv.notify_all();
        thread.join();
    }
};

int main(int argc, char const *argv[])
{
    FuturePool future_pool;
    ThreadPool thread_pool(10);
    for (size_t i = 0; i < 100; i++)
    {
        std::packaged_task<int(int)> package(task);
        std::future<int> future = package.get_future();
        future_pool.addFuture(std::move(future));
        auto package_ptr = std::make_shared<std::packaged_task<int(int)>>(std::move(package));
        thread_pool.addTask([package_ptr = std::move(package_ptr), i]() mutable { 
            (*package_ptr)(i); 
        });
    }

    std::cout << "completed" << std::endl;
    return 0;
}
