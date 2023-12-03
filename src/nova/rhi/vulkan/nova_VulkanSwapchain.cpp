#pragma once

#ifdef NOVA_DXGI_SWAPCHAIN
#  include "win32/nova_VulkanWin32Swapchain.inl"
#else
#  include "khr/nova_VulkanKhrSwapchain.inl"
#endif