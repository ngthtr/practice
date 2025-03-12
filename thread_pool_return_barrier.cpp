 
#include <iostream> 
#include <thread> 
#include <mutex> 
#include <condition_variable> 
#include <vector> 
#include <queue> 
#include <functional> 
#include <atomic> 
#include <future> 

class Barrier { 

private: 

    size_t number_threads; 
    std::atomic_int waiters = 0; 
    std::function<void()> task; 
    std::atomic_bool stop = false; 
 
    std::mutex barrier_mtx; 
    std::condition_variable barrier_cv; 
    std::vector<std::thread> pool_threads; 
 
    std::mutex result_mtx; 
    std::condition_variable result_cv; 
    std::thread result_thread; 
 
    std::queue<std::function<void()>> tasks; 
    std::queue<std::future<int>> results; 
 
private: 
    std::thread initializeResultThread() 
    { 
        std::thread thread([this]  
        { 
            while (true) 
            { 
                std::unique_lock<std::mutex> lock(result_mtx); 
                result_cv.wait(lock, [this] { 
                    return results.size() >= number_threads || stop; 
                }); 

                if (stop && results.empty()) return; 
                 
                // get result 

                std::cout << "start to get result\n"; 
                int result = 0; 
                for (size_t i = 0; i < number_threads; i++) 
                { 
                    std::future<int> future = std::move(results.front()); 
                    result += future.get(); 
                    results.pop(); 
                } 

                waiters = 0; 
                
                std::cout << "Completed get result = " << result << std::endl; 
                std::cout << "======== Wakeup all threads waiters barrier =======" << "\n"; 
                barrier_cv.notify_all(); 
            } 

        }); 
        return std::move(thread); 
    } 

    std::vector<std::thread> initializeThreadPool(int number_threads) 
    { 
        std::vector<std::thread> threads; 
        for (size_t i = 0; i < number_threads; ++i)  
        { 
            std::thread thread([this, number_threads]()  
            { 
                while (true) 
                { 
                    std::function<void()> task; 
                    { 
                        std::unique_lock<std::mutex> lock(barrier_mtx); 
                        barrier_cv.wait(lock, [this] { 
                            return !tasks.empty() || stop; 
                        }); 
                        
                        if (stop && tasks.empty()) return;  
                        
                        task = std::move(tasks.front()); 
                        tasks.pop(); 
                    } 

                    task(); 
     
                    waiters++; 
                    if (waiters == number_threads) 
                    { 
                        std::cout << "waiters enough all " << number_threads << " threads\n"; 
                        result_cv.notify_one(); 
                    } 

                    std::unique_lock<std::mutex> lock(barrier_mtx); 
                    barrier_cv.wait(lock, [this] { 
                        return waiters == 0 || stop;  
                    }); 
                     
                    if (stop && waiters == 0) return; 
                } 
            }); 
            threads.push_back(std::move(thread)); 
        } 
        return std::move(threads); 
    } 

public: 
    Barrier(size_t _number_threads) : number_threads(_number_threads)  
    { 
        result_thread = initializeResultThread(); 
        pool_threads = initializeThreadPool(number_threads); 
    } 

    void addTask(std::future<int>&& future, std::function<void()> task) 
    { 
        {
            std::unique_lock<std::mutex> lock(barrier_mtx); 
            tasks.push(std::move(task)); 
            results.push(std::move(future)); 
        } 
        barrier_cv.notify_one(); 
    } 

    ~Barrier() 
    { 
        std::cout << "Deconstructor Barrier\n"; 
        { 
            std::unique_lock<std::mutex> lock1(barrier_mtx); 
            std::unique_lock<std::mutex> lock2(result_mtx); 
            stop = true; 
        } 

        result_cv.notify_all(); 
        for (std::thread& thread : pool_threads) { 
            thread.join(); 
        } 
        barrier_cv.notify_all(); 
        result_thread.join(); 
    } 
}; 
 
int worker(int id)  
{ 
    std::this_thread::sleep_for(std::chrono::seconds(1)); // Simulate work 
    std::cout << "complete = " << id << "\n"; 
    return id; 
} 

int main() { 
    const size_t num_threads = 5; 
    Barrier barrier(num_threads); 

    for (size_t i = 1; i <= 20; i++) 
    { 
        std::packaged_task<int(int)> package(worker); 
        std::future<int> future = package.get_future(); 
        auto package_ptr = std::make_shared<std::packaged_task<int(int)>>(std::move(package)); 
        barrier.addTask(std::move(future), [id = i%5, package_ptr = std::move(package_ptr)] { 
            (*package_ptr)(id); 
        }); 
    } 
    return 0; 
} 

 