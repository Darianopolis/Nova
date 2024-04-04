# Nova

Nova is a collection of libraries for primarily graphics focused application development

## Nova :: Core

Common utilities, collections, macros, and 3rd party libraries.

## Nova :: RHI

An experimental Vulkan GPGPU library.

#### Stageless transfers
- Combines Resizable-BAR with VK_EXT_host_image_copy to elimate nearly all host -> device staging

#### Dynamic Rendering
- No renderpass/framebuffer management - just start rendering
- `[TODO]` Retain tile-based rendering optimizations via VK_KHR_dynamic_rendering_local_read and VK_EXT_shader_tile_image

#### Unified Bindless Resources
- No explicit descriptor management
- Image and sampler descriptors handled automatically
- All buffers handled with buffer device addresses
- Simply request bindless descriptors and pass into shaders as data

#### Dynamic draw state API
- No pipeline management
- Hash'n'cache supporting any mix of pipelines, pipeline libraries, and shader objects
- Sensible comprehensive default state
- `[TODO]` Allow static state to be requested for potential optimizations

#### Hardware Ray Tracing
- SBT table generation with sensible defaults
- Build and update supplementary hit group table on CPU or GPU
- Acceleration structure builder for building/updating/compacting/querying acceleration structures

#### Minimum requirements
- Vulkan 1.3
   - `(1.2)` Descriptor Indexing
   - `(1.2)` Timeline Semaphores
   - `(1.2)` Buffer Device Addresses
   - `(1.3)` Synchronization 2
   - `(1.3)` Dynamic Rendering

#### Recommended capabilities / extensions

- Full coherent host access to device heap (Resizable BAR or similar)
- VK_EXT_graphics_pipeline_library
- VK_EXT_descriptor_buffer
- VK_EXT_host_image_copy
- VK_EXT_shader_object

#### Additional supported extensions
- VK_EXT_mesh_shader
- VK_KHR_fragment_shader_barycentric
- VK_EXT_fragment_shader_interlock
- VK_KHR_ray_tracing_pipeline
- VK_KHR_acceleration_structure
- VK_KHR_ray_query
- VK_NV_ray_tracing_invocation_reorder
- VK_KHR_ray_tracing_position_fetch

## Nova :: Draw2D

Simple, work-in-progress immediate mode drawing library

- Procedural antialiased rounded quads
- Basic text rendering

## Nova :: Database

Tiny SQLite database wrapper
  - (Lot's of work to be done here)

## Nova :: Window

Windowing and input library
  - Planned cross-platform support (Windows only currently)
- `[TODO]` Extensible input system

## Nova :: Imgui

Simple ImGui backend for Nova::RHI and Nova::Window