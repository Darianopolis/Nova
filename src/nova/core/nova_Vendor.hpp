#pragma once

// -----------------------------------------------------------------------------

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include "Windows.h"

// -----------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable: 4201)

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/compatibility.hpp>

#pragma warning(pop)

// -----------------------------------------------------------------------------

#include <mimalloc.h>

#include <ankerl/unordered_dense.h>

// -----------------------------------------------------------------------------

inline
void* __cdecl operator new[](size_t size, const char* /* name */, int /* flags */, unsigned /* debugFlags */, const char* /* file */, int /* line */)
{
	return ::operator new(size);
}

inline
void* __cdecl operator new[](size_t size, size_t align, size_t /* ??? */, const char* /* name */, int /* flags */, unsigned /* debugFlags */, const char* /* file */, int /* line */)
{
    return ::operator new(size, std::align_val_t(align));
}