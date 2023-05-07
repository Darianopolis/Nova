# Pyrite

Pyrite is a rendering engine and editor built primarily for educational purposes.

- Built around Vulkan to run on Windows (and aspirationally Linux). As such it is heavily architected around dynamism, including dynamic rendering, descriptor buffers, and the newly released shader objects to fully take advantage of the highly dynamic capabilities of desktop GPUs.
- Built with newest features in mind, this engine is primarily a playground for implementing the newest tech. As such this engine only runs on cards with the highest level of extension support.
  - Full Vulkan 1.3 support
  - Dynamic rendering
  - Descriptor buffers
  - Ray tracing pipeline, queries, and acceleration structures
    - Additionally - shader invocation reordering
  - Shader objects
  - GPU Memory decompression and indirect memory copy

Current platform requirements
- Windows 11
- NVidia 40 series card

<br>

# Building

Dependencies:
- glfw
- vulkan sdk
  - shader objects require source built validation layers currently
- vulkan memory allocator
- glslang
- glm
- cgltf
- stb
- imgui
  - requires a custom fork with dynamic rendering support
- meshoptimizer
- ankerl-maps

Building is left as an exercise for the user. Good luck!

<br>

# Roadmap

- `[X]` Basic Editor layout setup
- `[X]` Primitive shapes, debug render
- `[X]` Gizmos
- `[X]` Mouse picking
- `[X]` Hierarchies
- `[X]` Ray trace debug
- `[ ]` Materials and basic lighting
- `[ ]` Physics
- `[ ]` Height map terrain
- `[ ]` glTF model loading
- `[ ]` Scene (de)serialization
- `[ ]` Outlines
- `[ ]` GPU cull and LOD
