#include "job_system.h"

void CJobManager::resize(int num_thread)
{
    if (!is_done)
    {
        int old_thd_num = static_cast<int>(threads.size());
        if (old_thd_num <= num_thread)
        {
            threads.resize(num_thread);
            //is_finished.resize(num_thread);
            for (int idx = old_thd_num; idx < num_thread; idx++)
            {
                setThread(idx);
            }
        }
        else
        {
            assert(false);
        }
    }
}

void CJobManager::stop()
{
    if (is_done)
    {
        return;
    }
    is_done = true;

    {
        std::unique_lock<std::mutex> lock(mutex_var);
        cv.notify_all();
    }

    for (int idx = 0; idx < getThreadSize(); idx++)
    {
        if (threads[idx]->joinable())
        {
            threads[idx]->join();
        }
    }

    clearJobQueue();
    threads.clear();
}

void CJobManager::setThread(int idx)
{
    auto thread_function = [this]()
    {
        SJob job;
        bool is_pop = job_queue.pop(job);
        while (true)
        {
            while (is_pop)
            {
                //std::unique_ptr<std::function<void(CJobData*)>> function_run(job.job_func);
                (*job.job_func)(job.job_data);
                is_pop = job_queue.pop(job);
            }

            std::unique_lock<std::mutex> lock(mutex_var);
            cv.wait(lock, [this, &job, &is_pop]
            {
                is_pop = job_queue.pop(job);
                return is_pop || this->is_done;
            });

            if (!is_pop)
            {
                return;
            }
        }
    };

    threads[idx].reset(new std::thread(thread_function));
}