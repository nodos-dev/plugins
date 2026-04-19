// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "WindowNode.h"
#include "GLFW/glfw3.h"
#if defined(WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux)
#define GLFW_EXPOSE_NATIVE_X11
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#else
#error "Unsupported platform"
#endif
#include "GLFW/glfw3native.h"

#include "Nodos/Utils/Stopwatch.hpp"
#include <cstdint>
#undef CreateSemaphore
namespace nos::experiment
{

void RegisterWindowNode(nosNodeFunctions* out)
{	
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("nos.experiment.Window"), WindowNode, out);
}

WindowNode::~WindowNode() 
{ 
	assert(Window == nullptr);
}

bool WindowNode::CreateSwapchain()
{
	nosSwapchainCreateInfo createInfo = {};
	createInfo.SurfaceHandle = Surface;
	createInfo.Extent = { 800, 600 };
	createInfo.PresentMode = NOS_PRESENT_MODE_FIFO;
	nosResult res = nosVulkan->CreateSwapchain(&createInfo, &Swapchain.GetStorage(), &FrameCount);
	if (res != NOS_RESULT_SUCCESS)
		return false;
	nosSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.Type = NOS_SEMAPHORE_TYPE_BINARY;
	Images.resize(FrameCount);
	if (FrameCount > 0)
		nosVulkan->GetSwapchainImages(Swapchain, &Images[0].GetStorage());
	WaitSemaphore.resize(FrameCount);
	SignalSemaphore.resize(FrameCount);
	for (int i = 0; i < FrameCount; i++)
	{
		nosVulkan->CreateSemaphore(&semaphoreCreateInfo, &WaitSemaphore[i].GetStorage());
		nosVulkan->CreateSemaphore(&semaphoreCreateInfo, &SignalSemaphore[i].GetStorage());
	}
	return true;
}

void WindowNode::Clear() 
{
	DestroySwapchain();
	DestroyWindowSurface();
	DestroyWindow();
}

void WindowNode::DestroySwapchain() 
{
	if (!Swapchain)
		return;
	nosCmd cmd;
	nosCmdBeginParams beginParams = {.Name = NOS_NAME("Window node flush cmd"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd};
	nosVulkan->Begin(&beginParams);
	nosGPUEvent wait;
	nosCmdEndParams endParams = {.ForceSubmit = true, .OutGPUEventHandle = &wait};
	nosVulkan->End(cmd, &endParams);
	nosVulkan->WaitGpuEvent(&wait, UINT64_MAX);
	WaitSemaphore.clear();
	SignalSemaphore.clear();
	Images.clear();
	Swapchain = {};
}

void WindowNode::DestroyWindowSurface() 
{
	Surface = {};
}

void WindowNode::DestroyWindow() 
{
	if (!Window)
		return;
	glfwDestroyWindow(Window);
	glfwTerminate();
	Window = nullptr;
}

nosResult WindowNode::ExecuteNode(NodeExecuteParams const& params)
{
	if (!Window)
		return NOS_RESULT_FAILED;
	nosScheduleNodeParams scheduleParams = {};
	scheduleParams.NodeId = NodeId;
	scheduleParams.Reset = false;
	scheduleParams.AddScheduleCount = 1;

	nos::NodeExecuteParams execParams = params;

	auto input = *execParams[NOS_NAME("Input")].Object;
	if (!input)
		return NOS_RESULT_FAILED;

	if(!glfwWindowShouldClose(Window))
	{
		glfwPollEvents();

		uint32_t imageIndex;
		nosVulkan->SwapchainAcquireNextImage(Swapchain, -1, &imageIndex, WaitSemaphore[CurrentFrame]);
		nosCmd cmd = sys::vulkan::BeginCmd(NOS_NAME("Window"), NodeId);
		nosVulkan->Copy(cmd, input, Images[imageIndex], 0);

		nosVulkan->ImageStateToPresent(cmd, Images[imageIndex]);
		nosVulkan->AddWaitSemaphoreToCmd(cmd, WaitSemaphore[CurrentFrame], 1);
		nosVulkan->AddSignalSemaphoreToCmd(cmd, SignalSemaphore[CurrentFrame], 1);

		nosCmdEndParams endParams{.ForceSubmit = true};
		nosVulkan->End(cmd, &endParams);
		nosVulkan->SwapchainPresent(Swapchain, imageIndex, SignalSemaphore[CurrentFrame]);
		nosEngine.ScheduleNode(&scheduleParams);
		CurrentFrame = (CurrentFrame + 1) % FrameCount;
	}
	else
	{
		Clear();
		return NOS_RESULT_FAILED;
	}

	return NOS_RESULT_SUCCESS;
}

void WindowNode::OnExitRunnerThread(nosExitRunnerThreadParams const& params) 
{ 
	if (!params.RunnerId)
		return;
	Clear(); 
}

void WindowNode::OnEnterRunnerThread(nosEnterRunnerThreadParams const& params)
{
	if (!params.RunnerId)
		return;
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	Window = glfwCreateWindow(800, 600, "NOSWindow", nullptr, nullptr);
	
	auto windowHandle =
#if defined(WIN32)
	glfwGetWin32Window(Window)
#elif defined(__linux)
	glfwGetX11Window(Window)
#elif defined(__APPLE__)
	glfwGetCocoaWindow(Window)
#else
#error "Unsupported platform"
#endif
	;
	if (nosVulkan->CreateWindowSurface((void*)windowHandle, &Surface.GetStorage()) != NOS_RESULT_SUCCESS)
	{
		DestroyWindow();
		return;
	}
	if (!CreateSwapchain())
	{
		DestroyWindowSurface();
		DestroyWindow();
		return;
	}

}

void WindowNode::OnPathStop() 
{
	nosCmd cmd;
	nosCmdBeginParams beginParams = { .Name = NOS_NAME("Window node flush cmd"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd };
	nosVulkan->Begin(&beginParams);
	nosGPUEvent wait;
	nosCmdEndParams endParams = { .ForceSubmit = true, .OutGPUEventHandle = &wait };
	nosVulkan->End(cmd, &endParams);
	nosVulkan->WaitGpuEvent(&wait, UINT64_MAX);
}

void WindowNode::OnPathStart()
{
	nosScheduleNodeParams params = {};
	params.NodeId = NodeId;
	params.Reset = false;
	params.AddScheduleCount = 1;

	nosEngine.ScheduleNode(&params);
}

}