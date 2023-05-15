#include "nova_Core.hpp"

namespace nova
{
    struct Job
    {
        void(*pfn)(void*);
        void* data;
        Job** pDependents;
        std::atomic<u16> remainingDependencies = 0;
        u16 dependentsCount;
        std::atomic<bool> done = false;

        template<class Fn>
        void Bind(Fn& fn, u32 dependencies, Job** dependents, u16 _dependentsCount)
        {
            pfn = +[](void* d) { (*(Fn*)d)(); };
            data = &fn;
            remainingDependencies = dependencies;
            pDependents = dependents;
            dependentsCount = _dependentsCount;
        }

        void Wait()
        {
            done.wait(false);
        }
    };

    struct JobSystem
    {
        std::vector<std::jthread> workers;
        std::queue<Job*> queue;
        std::shared_mutex mutex;
        std::condition_variable_any cv;

        bool running = true;

    public:
        JobSystem(u32 threads)
        {
            for (u32 i = 0; i < threads; ++i)
            {
                workers.emplace_back([&] {
                    Worker();
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

        void Worker()
        {
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
                queue.pop();

                // unlock
                lock.unlock();

                // run job
                job->pfn(job->data);
                job->done = true;
                job->done.notify_all();

                // fire off dependencies
                for (u32 i = 0; i < job->dependentsCount; ++i)
                {
                    if (--job->pDependents[i]->remainingDependencies == 0)
                        Submit(job->pDependents[i]);
                }
            }
        }

        void Submit(Job* job)
        {
            std::scoped_lock lock { mutex };
            queue.push(std::move(job));
            cv.notify_one();
        }
    };
}