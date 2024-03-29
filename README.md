# Nova

Nova is a modular framework for (graphical) application development

# Nova :: Core

A core collection of common utilities, collections, macros, and 3rd party libraries for application development.

# Nova :: RHI

Nova's RHI is a highly experimental take on a Vulkan GPGPU framework, intended as a sandbox to play around with all the latest shiny new Vulkan extensions.

#### Stageless transfers
- Combines Resizable-BAR with VK_EXT_host_image_copy to elimate nearly all host -> device staging

#### Dynamic Rendering
- No renderpass/framebuffer management - just start rendering
- `[TODO]` Retain tile-based rendering optimizations via VK_EXT_shader_tile_image

#### Unified Bindless Resources
- No explicit descriptor management
- Image and sampler descriptors handled automatically
- Simply request bindless descriptors and pass into shaders as data

#### Dynamic drawing API
- No pipeline management
- Hash'n'cache supporting any mix of pipelines, pipeline libraries, and shader objects 
- Sensible comprehensive default state
- Convenient grouped pipeline state updates

#### Convenient HWRT
- Neatly wrapped ray tracing pipeline management
- Automatic base shader binding table construction
- Build and update supplementary hit group table on CPU or GPU
- Acceleration structure builder for building/updating/compacting acceleration structures efficiently

#### Minimum requirements
- Vulkan 1.3

#### Recommended extensions
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

# Nova :: Draw2D

- Tiny immediate mode API drawing library
- Procedural antialiased rounded quads
- Basic text rendering

# Nova :: Database

- Tiny SQLite database wrapper
  - (Lot's of work to be done here)

# Nova :: Window

- Windowing and input library
  - Planned cross-platform support (Windows only currently)

# Nova :: Imgui

- ImGui backend for Nova::RHI and Nova::Window

# Components

- `[X]` Core utilities
- `[X]` Sqlite
- `[X]` RHI
- `[ ]` 2D Renderer
  - `[X]` Rounded rectangles
  - `[X]` Basic text
  - `[ ]` Lazily rasterized fonts
- `[X]` Windowing
- `[X]` ImGui backend
- `[ ]` Model loading & processing
  - `[ ]` Frontends
    - `[ ]` Wavefront obj
    - `[ ]` FBX
    - `[ ]` glTF
    - `[ ]` Fallback loader (Assimp)
  - `[ ]` Processing
    - `[ ]` Image resizing/filtering
    - `[ ]` Image transcoding
    - `[ ]` Image mipmap generation
    - `[ ]` Mesh meshletization
    - `[ ]` Mesh simplification
    - `[ ]` Mesh quantization
  - `[ ]` 3D Rendering framework
    - `[ ]` ???