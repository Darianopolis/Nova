# Nova

Nova is a highly experimental take on a Vulkan GPGPU framework, intended as a sandbox to play around with all the latest shiny new Vulkan extensions.

 - Descriptor Buffers
   - Optimized for bindless rendering with BDAs
 - Shader Objects
   - No explicit raster/compute pipelines
 - Ray Tracing
   - Combined SBT and pipelines
   - Acceleration structure builders

# Requirements

GPU vendor:
 - Nvidia
   - Only Nvidia currently tested or supported
   - Several extensions (current or planned) are currently Nvidia exclusive, or optimal for Nvidia. Better support for AMD later (maybe)

Driver and API:
- Vulkan 1.3
 - Use latest Nvidia Vulkan beta drivers for best effect

Core device extensions:
 - VK_KHR_swapchain
 - VK_EXT_shader_object
 - VK_EXT_descriptor_buffer
 - VK_KHR_push_descriptor
 - VK_KHR_deferred_host_operations
 - VK_KHR_pipeline_library
 - VK_KHR_fragment_shader_barycentrics

Ray Tracing device extensions:
 - VK_KHR_ray_tracing_pipeline
 - VK_KHR_acceleration_structure
 - VK_KHR_ray_query
 - VK_NV_ray_tracing_invocation_reorder
   - Ray tracing with SER requires an NVidia GPU
 - VK_KHR_ray_tracing_position_fetch

# Building

Core Dependencies:
- Vulkan SDK
- Vulkan Memory Allocator
- volk
- glslang
- glm
- ankerl-maps

Extra Dependencies:
- glfw
- stb
- imgui
- sqlite (don't ask)

Building is left as an exercise for the user. Good luck!

# To Do

1) Make it work
2) Make it simple (beautiful)
3) Make it fast
3) Get bored and start again