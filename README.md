# nova

Nova is a set of libraries for supporting application development, with a focus on graphical applications.

  - Core
  - OS - Platform abstraction layer (WIP)
  - RHI - API-agnostic RHI implenentation
    - Vulkan - Yes
    - DX12 - Planned
  - UI
    - ImGui backend
    - ImDraw - 2D drawing library

# nova::rhi

Nova's RHI is a highly experimental take on a Vulkan GPGPU framework, intended as a sandbox to play around with all the latest shiny new Vulkan extensions.

 - Descriptor management
   - Optimized for bindless rendering with BDAs
   - Supports both traditional Descriptor Sets and Descriptor Buffers (no debugger support yet)
 - Automatic pipeline management
   - Simply set pipeline state and shaders at recording time
   - Nova transparently uses dynamic state and pipeline libraries to manage pipeline permutations
   - Optional use of shader objects to avoid pipeline creation entirely (no debugger support yet)
 - Ray Tracing
   - Combined SBT and pipelines
   - Acceleration structure builders

# 3rd Party Dependencies

  - Core: ankerl-maps, glm, mimalloc
  - DB: sqlite3
  - RHI: vulkan, VMA, glslang, volk
  - UI: freetype, glfw, imgui, imguizmo