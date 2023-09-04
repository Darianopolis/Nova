# nova

Nova is a set of libraries for supporting application development, with a focus on graphical applications.

# nova::core

A core collection of common utilities, collections, macros, and 3rd party libraries for application development.

# nova::rhi

Nova's RHI is a highly experimental take on a Vulkan GPGPU framework, intended as a sandbox to play around with all the latest shiny new Vulkan extensions.

#### Stageless transfers
- Combines Resizable-BAR with VK_EXT_host_image_copy to elimate nearly all host -> device staging

#### Dynamic Rendering
- No renderpass/framebuffer management - just start rendering
- `[TODO]` Retain tile-based rendering optimizations via VK_EXT_shader_tile_image

#### Unified Descriptor Heap
- No explicit descriptor set/layout management
- Using VK_EXT_mutable_descriptor_type to achieve DirectX style descriptor heaps optimal for bindless

#### Dynamic drawing API
- No pipeline management
- Native pipeline-less support with VK_EXT_shader_object
- Update dynamic state efficiently for more efficient small pipeline state switches

#### Convenient HWRT
- Neatly wrapped ray tracing pipeline management
- Automatic shader binding table updates
- Acceleration structure builder for building/updating/compacting acceleration structures efficiently

#### Minimum requirements:
- Vulkan 1.3
- VK_EXT_mutable_descriptor_type
- VK_EXT_host_image_copy
- VK_EXT_shader_object

# nova::ui

#### ImDraw2D
- Tiny immediate mode drawing library
- Procedural antialiased rounded quads
- Basic text rendering

#### ImGuiLayer
- Tiny ImGui backend
- Integrates glfw + custom drawing backend
