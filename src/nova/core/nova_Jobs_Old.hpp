// #pragma once

// #include "nova_Core.hpp"

// namespace nova
// {
//     struct Semaphore
//     {
//         std::atomic<u32> counter = 0;

//         void Wait(u32 target = 0)
//         {
//             u32 v = counter.load();
//             while (v != target)
//             {
//                 counter.wait(v);
//                 v = counter.load();
//             }
//         }
//     };

//     struct Job
//     {
//         void(*pfn)(void*);
//         void* data;
//         Job** pDependents;
//         Semaphore* pSignal = {};
//         u16 remainingDependencies = 0;
//         u16 dependentsCount = 0;

//         template<class Fn>
//         void Bind(Fn& fn, u32 dependencies, Job** dependents, u16 _dependentsCount)
//         {
//             pfn = +[](void* d) { (*static_cast<Fn*>(d))(); };
//             data = &fn;
//             remainingDependencies = dependencies;
//             pDependents = dependents;
//             dependentsCount = _dependentsCount;
//         }
//     };

//     struct JobSystem
//     {
//         std::vector<std::jthread> workers;
//         std::queue<Job*> queue;
//         std::shared_mutex mutex;
//         std::condition_variable_any cv;

//         bool running = true;

//     public:
//         JobSystem(u32 threads)
//         {
//             for (u32 i = 0; i < threads; ++i)
//             {
//                 workers.emplace_back([&] {
//                     Worker();
//                 });
//             }
//         }

//         ~JobSystem()
//         {
//             Shutdown();
//         }

//         void Shutdown()
//         {
//             std::scoped_lock lock{mutex};
//             running = false;
//             cv.notify_all();
//         }

//         void Worker()
//         {
//             for (;;)
//             {
//                 // Acquire a lock
//                 std::unique_lock lock{mutex};

//                 // Wait while the queue is empty
//                 while (queue.empty())
//                 {
//                     if (!running)
//                         return;
//                     cv.wait(lock); // Wait for cv.notify_one() that signals a job has been added
//                 }

//                 // Pop from the queue (while owning the lock)
//                 auto job = queue.front();
//                 queue.pop();

//                 // unlock
//                 lock.unlock();

//                 // run job
//                 job->pfn(job->data);
//                 if (job->pSignal && --job->pSignal->counter == 0)
//                     job->pSignal->counter.notify_all();

//                 // fire off dependencies
//                 for (u32 i = 0; i < job->dependentsCount; ++i)
//                 {
//                     if (--std::atomic_ref(job->pDependents[i]->remainingDependencies) == 0)
//                         Submit(job->pDependents[i]);
//                 }
//             }
//         }

//         void Submit(Job* job)
//         {
//             std::scoped_lock lock { mutex };
//             queue.push(job);
//             cv.notify_one();
//         }

//         template<class Fn>
//         void SubmitFn(Fn&& fn, Semaphore* pSignal = nullptr)
//         {
//             struct HeapJob { Fn fn; Job job = {};};
//             auto job = new HeapJob{std::move(fn)};
//             job->job.data = job;
//             job->job.pfn = +[](void* d) {
//                 auto self = static_cast<HeapJob*>(d);
//                 self->fn();
//                 delete self;
//             };
//             job->job.pSignal = pSignal;
//             ++pSignal->counter;

//             Submit(&job->job);
//         }
//     };
// }