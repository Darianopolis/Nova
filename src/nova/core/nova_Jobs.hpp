#pragma once

#include "nova_Core.hpp"

namespace nova
{
    struct Job;

    struct Barrier : std::enable_shared_from_this<Barrier>
    {
        std::atomic<u32> counter = 0;
        u32 acquired = 0;
        std::vector<std::shared_ptr<Job>> pending;

        // void Signal()
        // {
        //     if (--counter == 0)
        //     {
        //         for (auto& task : pending)
        //         {
        //             task->system->Submit(task);
        //         }
        //         counter.notify_all();
        //     }
        // }

        void Wait()
        {
            u32 v = counter.load();
            while (v != 0)
            {
                counter.wait(v);
                v = counter.load();
            }
        }

        std::shared_ptr<Barrier> Acquire(u32 count = 1)
        {
            counter += count;
            acquired += count;
            return shared_from_this();
        }

        std::shared_ptr<Barrier> Add(std::shared_ptr<Job> job)
        {
            pending.push_back(std::move(job));
            return shared_from_this();
        }

        static std::shared_ptr<Barrier> Create()
        {
            return std::make_shared<Barrier>();
        }
    };

    struct JobSystem;

    struct Job : std::enable_shared_from_this<Job>
    {
        JobSystem* system = {};
        std::function<void()> task;
        std::vector<std::shared_ptr<Barrier>> signals;

        static std::shared_ptr<Job> Create(JobSystem* system, std::function<void()> task)
        {
            auto job = std::make_shared<Job>();
            job->system = system;
            job->task = std::move(task);
            return job;
        }

        std::shared_ptr<Job> Signal(std::shared_ptr<Barrier> signal)
        {
            if (signal->acquired > 0)
            {
                signal->acquired--;
            }
            else
            {
                signal->counter++;
            }
            signals.emplace_back(std::move(signal));
            return shared_from_this();
        }

        void Submit();

        // Job** pDependents;
        // Semaphore* pSignal = {};
        // u16 remainingDependencies = 0;
        // u16 dependentsCount = 0;

        // template<class Fn>
        // void Bind(Fn& fn, u32 dependencies, Job** dependents, u16 _dependentsCount)
        // {
        //     pfn = +[](void* d) { (*static_cast<Fn*>(d))(); };
        //     data = &fn;
        //     remainingDependencies = dependencies;
        //     pDependents = dependents;
        //     dependentsCount = _dependentsCount;
        // }
    };

    struct WorkerState
    {
        u32 workerID = ~0u;
    };

    inline thread_local WorkerState JobWorkerState;

    struct JobSystem
    {
        std::vector<std::jthread> workers;
        std::deque<std::shared_ptr<Job>> queue;
        std::shared_mutex mutex;
        std::condition_variable_any cv;

        bool running = true;

    public:
        JobSystem(u32 threads)
        {
            for (u32 i = 0; i < threads; ++i)
            {
                workers.emplace_back([this, i] {
                    Worker(this, i);
                });
            }
        }

        ~JobSystem()
        {
            Shutdown();
        }

        void Shutdown()
        {
            std::scoped_lock lock{mutex};
            running = false;
            cv.notify_all();
        }

        static u32 GetWorkerID() noexcept
        {
            return JobWorkerState.workerID;
        }

        void Worker([[maybe_unused]] JobSystem* system, u32 index)
        {
            JobWorkerState.workerID = index;
            NOVA_ON_SCOPE_EXIT() { JobWorkerState.workerID = ~0u; };

            for (;;)
            {
                // Acquire a lock
                std::unique_lock lock{mutex};

                // Wait while the queue is empty
                while (queue.empty())
                {
                    if (!running)
                        return;
                    cv.wait(lock); // Wait for cv.notify_one() that signals a job has been added
                }

                // Pop from the queue (while owning the lock)
                auto job = queue.front();
                queue.pop_front();

                // unlock
                lock.unlock();

                // run job
                job->task();

                for (auto& signal : job->signals)
                {
                    // signal->Signal();
                    // NOVA_LOG("Signalling, counter = {}", signal->counter.load());
                    if (--signal->counter == 0)
                    {
                        // NOVA_LOG("  Reached 0!");
                        for (auto& task : signal->pending)
                        {
                            task->system->Submit(task, true);
                        }
                        signal->counter.notify_all();
                    }
                }
            }
        }

        void Submit(std::shared_ptr<Job> job, bool front = false)
        {
            std::scoped_lock lock { mutex };
            if (front)
                queue.push_front(job);
            else
                queue.push_back(job);
            cv.notify_one();
        }
    };

    inline
    void Job::Submit()
    {
        system->Submit(shared_from_this());
    }
}