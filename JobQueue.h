#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

class JobQueue 
{
public:
    using Job = std::function<void()>;

    void push(Job job) 
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_jobs.push(std::move(job));
        }
        m_cv.notify_one();
    }

    bool pop(Job& out) 
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_jobs.empty() || m_shutdown; });

        if (m_jobs.empty()) {
            return false;
        }
        out = std::move(m_jobs.front());
        m_jobs.pop();
        return true;
    }

    void shutdown() 
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_shutdown = true;
        }
        m_cv.notify_all();
    }

private:
    std::queue<Job> m_jobs;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_shutdown = false;
};