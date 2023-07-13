#pragma once

#include "nova_Core.hpp"
#include "nova_Ref.hpp"

namespace nova
{
    struct Job;

    struct Barrier : RefCounted
    {
        std::atomic<u32> counter = 0;
        u32 acquired = 0;
        std::vector<Ref<Job>> pending;

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

        Ref<Barrier> Acquire(u32 count = 1)
        {
            counter += count;
            acquired += count;
            return this;
        }

        Ref<Barrier> Add(Ref<Job> job)
        {
            pending.push_back(std::move(job));
            return this;
        }

        static Ref<Barrier> Create()
        {
            return new Barrier();
        }
    };

    struct JobSystem;

    struct Job : RefCounted
    {
        JobSystem* system = {};
        std::function<void()> task;
        std::vector<Ref<Barrier>> signals;

        static Ref<Job> Create(JobSystem* system, std::function<void()> task)
        {
            Ref job = new Job();
            job->system = system;
            job->task = std::move(task);
            return job;
        }

        Ref<Job> Signal(Ref<Barrier> signal)
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
            return this;
        }

        void Submit();

        // Job** pDependents;
        // Semaphore* pSignal = {};
        // u16 remainingDependencies = 0;
        // u16 dependentsCount = 0;

        // template<typename Fn>
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
        std::deque<Ref<Job>> queue;
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

        void Submit(Ref<Job> job, bool front = false)
        {
            std::scoped_lock lock { mutex };
            if (front)
                queue.push_front(std::move(job));
            else
                queue.push_back(std::move(job));
            cv.notify_one();
        }
    };

    inline
    void Job::Submit()
    {
        system->Submit(this);
    }
}