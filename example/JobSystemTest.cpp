#include <nova/core/nova_Jobs.hpp>

#include <nova/core/nova_Timer.hpp>
#include <nova/core/nova_Registry.hpp>

#include <cmath>

using namespace nova;

thread_local Timer timer;

void TestColony()
{
    ConcurrentDynamicArray<u32> colony;

    std::atomic<bool> running = true;
    std::atomic<usz> size = 0;

    using namespace std::chrono;
    auto start = steady_clock::now();
    usz totalReads = 0;

    {
        std::jthread t{[&] {
            while (running)
            {
                usz start = size.load();
                totalReads += start;
                std::this_thread::sleep_for(1ms);
                // NOVA_LOG("Running test, size = {}", size.load());
                for (i64 i = start - 1; i >= 0; --i)
                {
                    if (colony[i] != i)
                    {
                        NOVA_LOG("Invalid value at {} = {}", i, colony[i]);
                        // return;
                    }
                }
            }
        }};

        for (u32 i = 0; i < 100'000'000; ++i)
        {
            colony.EmplaceBack().second = i;
            size++;
        }

        NOVA_LOG("Waiting");

        std::this_thread::sleep_for(1s);
        // NOVA_LOG("Running test, size = {}", colony.GetSize());
        // for (i64 i = colony.GetSize(); i >= 0; --i)
        // {
        //     if (colony[i] != i)
        //     {
        //         NOVA_LOG("Invalid value at {} = {}", i, colony[i]);
        //         // return;
        //     }
        // }

        NOVA_LOG("Shutting down");
        running = false;
    }

    auto delta = steady_clock::now() - start;
    auto nanos = duration_cast<nanoseconds>(delta).count();
    NOVA_LOG("Total reads = {}", totalReads);
    NOVA_LOG("Total time = {} ({:.2f} s)", nanos, duration_cast<duration<double>>(delta).count());
    NOVA_LOG("Reads per nanosecond = {}", totalReads / nanos);
    NOVA_LOG("Closed");
}

void TestJobSystem()
{
    std::mt19937 rng{std::random_device{}()};

    JobSystem jobSystem{28};

    JobSystem uploadQueue{1};

    struct Texture : RefCounted
    {
        Ref<Job> fileLoadJob;

        Ref<Barrier> wait;
        Ref<Job> gpuLoadJob;
    };

    struct Material : RefCounted
    {
        std::vector<Ref<Texture>> textures;

    public:
        Ref<Barrier> wait;
        Ref<Job> job;
    };

    struct Mesh : RefCounted
    {
        Ref<Job> fileLoadJob;

        Ref<Barrier> wait;
        Ref<Job> gpuLoadJob;
    };

    struct MeshInstance : RefCounted
    {
        Ref<Mesh> mesh;
        Ref<Material> material;

    public:
        Ref<Barrier> wait;
        Ref<Job> job;
    };

    std::vector<Ref<Texture>> textures;
    std::vector<Ref<Material>> materials;
    std::vector<Ref<Mesh>> meshes;
    std::vector<Ref<MeshInstance>> meshInstances;

    auto pickRandom = [&](auto& vec) -> decltype(auto) {
        return vec[std::uniform_int_distribution<size_t>(0, vec.size() - 1)(rng)];
    };

    for (u32 i = 0; i < 249; ++i)
    // for (u32 i = 0; i < 10; ++i)
    {
        auto& texture = textures.emplace_back(std::make_shared<Texture>());

        texture->fileLoadJob = Job::Create(&jobSystem, [=] {
            auto target = std::chrono::steady_clock::now() + 25ms;
            while (timer.Wait(target - std::chrono::steady_clock::now(), true));
            NOVA_LOG("Loaded texture: {}", (void*)texture.Raw());
        });

        texture->gpuLoadJob = Job::Create(&uploadQueue, [=] {
            auto target = std::chrono::steady_clock::now() + 1ms;
            while (timer.Wait(target - std::chrono::steady_clock::now(), true));
            NOVA_LOG("transfer texture to gpu: {}", (void*)texture.Raw());
        });

        texture->wait = Barrier::Create();
        texture->wait->pending.push_back(texture->gpuLoadJob);

        texture->fileLoadJob->Signal(texture->wait);
    }

    ankerl::unordered_dense::set<u64> used;

    // for (u32 i = 0; i < 2491; ++i)

    // for (u32 i = 0; i < 100; ++i)
    {
        auto& material = materials.emplace_back(std::make_shared<Material>());

        material->job = Job::Create(&uploadQueue, [=] {
            auto target = std::chrono::steady_clock::now() + 1ms;
            while (timer.Wait(target - std::chrono::steady_clock::now(), true));
            NOVA_LOG("Loaded material: {}", (void*)material.Raw());
        });

        material->wait = Barrier::Create();
        material->wait->pending.push_back(material->job);

        // used.clear();
        // while (material->textures.size() < 4)
        // {
        //     auto& texture = pickRandom(textures);

        //     if (used.contains(u64(texture.get())))
        //         continue;
        //     used.insert(u64(texture.get()));

        //     texture->gpuLoadJob->AddSignal(material->wait);
        //     material->textures.push_back(texture);
        // }
        for (auto& texture : textures)
        {
            texture->gpuLoadJob->Signal(material->wait);
            material->textures.push_back(texture);
        }
    }

    for (u32 i = 0; i < 2774; ++i)
    // for (u32 i = 0; i < 100; ++i)
    {
        auto& mesh = meshes.emplace_back(std::make_shared<Mesh>());

        mesh->fileLoadJob = Job::Create(&jobSystem, [=] {
            auto target = std::chrono::steady_clock::now() + 25ms;
            while (timer.Wait(target - std::chrono::steady_clock::now(), true));
            NOVA_LOG("Loaded mesh: {}", (void*)mesh.Raw());
        });

        mesh->gpuLoadJob = Job::Create(&uploadQueue, [=] {
            auto target = std::chrono::steady_clock::now() + 1ms;
            while (timer.Wait(target - std::chrono::steady_clock::now(), true));
            NOVA_LOG("Uploaded mesh to gpu: {}", (void*)mesh.Raw());
        });

        mesh->wait = Barrier::Create();
        mesh->wait->pending.push_back(mesh->gpuLoadJob);

        mesh->fileLoadJob->Signal(mesh->wait);
    }

    auto completed = Barrier::Create();

    for (u32 i = 0; i < 65035; ++i)
    // for (u32 i = 0; i < 1000; ++i)
    {
        auto& instance = meshInstances.emplace_back(Ref<MeshInstance>::Create());

        instance->job = Job::Create(&jobSystem, [=] {
            NOVA_LOG("Loaded instance: {}", (void*)instance.Raw());
        });

        instance->wait = Barrier::Create();
        instance->wait->pending.push_back(instance->job);

        instance->job->Signal(completed);

        instance->mesh = pickRandom(meshes);
        instance->mesh->gpuLoadJob->Signal(instance->wait);

        instance->material = pickRandom(materials);
        instance->material->job->Signal(instance->wait);
    }

// -----------------------------------------------------------------------------


    using namespace std::chrono;
    auto start = steady_clock::now();

    // for (auto& mesh : meshes)
    // {
    //     jobSystem.Submit(mesh->fileLoadJob);
    // }

    // for (auto& texture : textures)
    // {
    //     jobSystem.Submit(texture->fileLoadJob);
    // }

    for (u32 i = 0; i < std::max(textures.size(), meshes.size()); ++i)
    {
        if (i < textures.size())
            textures[i]->fileLoadJob->Submit();

        if (i < meshes.size())
            meshes[i]->fileLoadJob->Submit();
    }

    // for (u32 i = 0; i < 65536; ++i)
    // {
    //     auto job = std::make_shared<Job>();
    //     job->system = &uploadQueue;
    //     job->task = [=] {
    //         // std::this_thread::sleep_for(1ms);
    //         // std::ostringstream oss;
    //         // oss << std::this_thread::get_id();
    //         NOVA_LOG("Job - {}", i);
    //     };
    //     job->AddSignal(completed);
    //     uploadQueue.Submit(job);
    // }

    NOVA_LOG("Waiting for completion");

    completed->Wait();

    auto dur = steady_clock::now() - start;
    std::cout << "Tasks completed in " << duration_cast<milliseconds>(dur) << '\n';

    std::cout << "All jobs completed\n";
}

int main()
{
    try
    {
        TestColony();
    }
    catch(...)
    {

    }
}