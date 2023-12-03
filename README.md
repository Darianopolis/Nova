# nova

Nova is a set of libraries for supporting application development, with a focus on graphical applications.

# nova::core

A core collection of common utilities, collections, macros, and 3rd party libraries for application development.

# nova::rhi

Nova's RHI is a highly experimental take on a Vulkan GPGPU framework, intended as a sandbox to play around with all the latest shiny new Vulkan extensions.

#### Principles
- Design for the future
  - Build based on the current direction of hardware evolution.
  - Don't compromise the API to support older devices.
- Doing **more** *per* call is better than making **more** calls.
  - Provide the user sufficient tools to perform bulk operations with minimal API calls ...
  - ... rather than hyperfocusing on reducing individual call overhead.
- Modern hardware and software are trending towards more flexible, dynamic architectures.
  - Architectures are becoming exponentially more complex with massive states.
  - Focus on providing efficient dynamic runtime operation first.
  - Provide options to declare/limit state ahead of time where *most valuable*.

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
- Native pipeline-less support with VK_EXT_shader_object
- Sensible comprehensive default state
- Convenient grouped pipeline state updates

#### Convenient HWRT
- Neatly wrapped ray tracing pipeline management
- Automatic base shader binding table construction
- Build and update supplementary hit group table on CPU or GPU
- Acceleration structure builder for building/updating/compacting acceleration structures efficiently

#### Minimum requirements
- Vulkan 1.3
- VK_EXT_graphics_pipeline_library

#### Recommended extensions
- Full coherent host access to device heap (Resizable BAR or similar)
- VK_EXT_host_image_copy

#### Additional supported extensions
- VK_EXT_descriptor_buffer
- VK_EXT_shader_object
- VK_EXT_mesh_shader
- VK_KHR_fragment_shader_barycentric
- VK_KHR_ray_tracing_pipeline
- VK_KHR_acceleration_structure
- VK_KHR_ray_query
- VK_NV_ray_tracing_invocation_reorder
- VK_KHR_ray_tracing_position_fetch

# nova::ui

#### Draw2D
- Tiny immediate mode API drawing library
- Procedural antialiased rounded quads
- Basic text rendering

#### ImGuiLayer
- Tiny ImGui backend
- Integrates glfw + custom drawing backend

# Components

  `[ ]` Core utilities
  `[X]` Sqlite
  `[ ]` RHI
  `[ ]` 2D Renderer
      `[X]` Rounded rectangles
      `[X]` Basic text
      `[ ]` Lazily rasterized fonts
  `[ ]` Windowing
  `[ ]` ImGui backend
  `[ ]` Model loading & processing
      `[ ]` Frontends
          `[ ]` Wavefront obj
          `[ ]` FBX
          `[ ]` glTF
          `[ ]` Fallback loader (Assimp)
      `[ ]` Processing
          `[ ]` Image resizing/filtering
          `[ ]` Image transcoding
          `[ ]` Image mipmap generation
          `[ ]` Mesh meshletization
          `[ ]` Mesh simplification
          `[ ]` Mesh quantization