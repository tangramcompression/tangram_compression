
#pragma once
#include <thread>
#include <mutex>
#include <atomic>

#include <boost/lockfree/queue.hpp>

#define DEFAULT_JOB_QUEUE_SIZE 64

class CJobData
{
public:
};

struct SJob
{
    std::function<void(CJobData*)>* job_func;
    CJobData* job_data;
};

class CJobManager
{
public:
    CJobManager() :job_queue(DEFAULT_JOB_QUEUE_SIZE) { init(); }
    CJobManager(int num_thread) :job_queue(DEFAULT_JOB_QUEUE_SIZE)
    {
        init();
        resize(num_thread);
    }

    ~CJobManager()
    {
        stop();
    }

    void resize(int num_thread);
    void stop();

    inline void addJob(std::function<void(CJobData*)>* job_func, CJobData* job_data)
    {
        job_queue.push(SJob{ job_func,job_data });
        std::unique_lock<std::mutex> lock(mutex_var);
        cv.notify_one();
        return;
    }

private:
    CJobManager(const CJobManager&);
    CJobManager(CJobManager&&);
    CJobManager& operator=(const CJobManager&);
    CJobManager& operator=(CJobManager&&);

    inline int getThreadSize() { return static_cast<int>(threads.size()); };

    inline void init() { is_done = false; }

    inline void clearJobQueue()
    {
        SJob job;
        while (job_queue.pop(job)) {}
    }

    void setThread(int idx);
    

    std::atomic<bool> is_done;
    std::vector<std::unique_ptr<std::thread>> threads;

    std::mutex mutex_var;
    std::condition_variable cv;

    mutable boost::lockfree::queue<SJob> job_queue;
};