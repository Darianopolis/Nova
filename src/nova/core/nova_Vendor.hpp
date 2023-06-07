#pragma once

// -----------------------------------------------------------------------------

#include <volk.h>

#include <vk_mem_alloc.h>

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
