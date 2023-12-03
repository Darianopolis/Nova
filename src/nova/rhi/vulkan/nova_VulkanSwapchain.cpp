#pragma once

#define NOVA_DXGI_SWAPCHAIN

#ifdef NOVA_DXGI_SWAPCHAIN
#  include "win32/nova_VulkanWin32Swapchain.inl"
#else
#  include "khr/nova_VulkanKHRSwapchain.inl"
#endif