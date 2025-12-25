/*
 * Copyright MediaZ Teknoloji A.S. All Rights Reserved.
 */

#pragma once

#include "Nodos/Plugin.hpp"
#include "nosSysVulkan/Helpers.hpp"

#include "GLFW/glfw3.h"

namespace nos::experiment
{
void RegisterWindowNode(nosNodeFunctions* out);
class WindowNode : public nos::NodeContext
{
public:
	~WindowNode();
	bool CreateSwapchain();
	void Clear();
	void DestroySwapchain();
	void DestroyWindowSurface();
	void DestroyWindow();

	nosResult ExecuteNode(NodeExecuteParams const& params) override;

	void OnPathStop() override;
	void OnPathStart() override;

	void OnEnterRunnerThread(nosEnterRunnerThreadParams const& params) override;
	void OnExitRunnerThread(nosExitRunnerThreadParams const& params) override;

private:
	GLFWwindow* Window = nullptr;
	std::vector<TypedObjectRef<nos::sys::vulkan::Semaphore>> WaitSemaphore{};
	std::vector<TypedObjectRef<nos::sys::vulkan::Semaphore>> SignalSemaphore{};
	std::vector<TypedObjectRef<sys::vulkan::Texture>> Images{};
	uint32_t FrameCount = 0;
	uint32_t CurrentFrame = 0;
	TypedObjectRef<nos::sys::vulkan::Surface> Surface{};
	TypedObjectRef<nos::sys::vulkan::Swapchain> Swapchain{};
};
}