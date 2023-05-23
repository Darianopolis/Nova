#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#define VOLK_IMPLEMENTATION
#include <volk.h>

// -----------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable: 4100)
#pragma warning(disable: 4189)
#pragma warning(disable: 4127)

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#pragma warning(pop)

// -----------------------------------------------------------------------------

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// -----------------------------------------------------------------------------

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

// -----------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable: 4559)
#include <mimalloc-new-delete.h>
#pragma warning(pop)