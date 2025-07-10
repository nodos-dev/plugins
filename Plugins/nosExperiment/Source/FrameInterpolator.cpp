// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>

#include <nosVulkanSubsystem/Helpers.hpp>

#include <shared_mutex>
#include "FrameInterpolator_generated.h"

namespace nos::test
{
NOS_REGISTER_NAME(Input)
NOS_REGISTER_NAME(Output)
NOS_REGISTER_NAME(Method)
NOS_REGISTER_NAME_SPACED(ClassName_FrameInterpolator, "nos.experiment.FrameInterpolator")
NOS_REGISTER_NAME(FrameInterpolator_BasicInterpolationShader)
NOS_REGISTER_NAME(FrameInterpolator_BasicInterpolationPass)

struct FrameInterpolatorNode : NodeContext
{
	nosResult OnCreate(nosFbNodePtr node) override
	{
		Thread = std::jthread([this]() { InterpolatorThread(); });
		return NOS_RESULT_SUCCESS;
	}

	~FrameInterpolatorNode()
	{
		ShouldStop = true;
	}

	nosResult CopyFrom(nosCopyInfo* copyInfo) override
	{
		return NOS_RESULT_SUCCESS;
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		NodeExecuteParams execParams(params);
		{
			std::unique_lock guard(Mutex);
			DeltaNanosec = 1'000'000'000u * execParams.GetDeltaTime();
			if (!InputPinId)
				InputPinId = execParams[NSN_Input].Id;
		}
		auto inputTextureInfo = vkss::DeserializeTextureInfo(execParams[NSN_Input].Data->Data);
		auto outputTextureInfo = vkss::DeserializeTextureInfo(execParams.GetPinData<void>(NSN_Output));
		auto method = execParams.GetPinData<nos::test::FrameInterpolationMethod>(NSN_Method);
		switch (*method)
		{
		case FrameInterpolationMethod::REPEAT: {
			nosCmd cmd = vkss::BeginCmd(NOS_NAME("Frame Interpolator"), NodeId);
			nosVulkan->Copy(cmd, &inputTextureInfo, &outputTextureInfo, 0);
			nosVulkan->End(cmd, NOS_FALSE);
			break;
		}
		default:
			//TODO:
			break;
		}
		return NOS_RESULT_SUCCESS;
	}

	void InterpolatorThread()
	{
		std::chrono::steady_clock::time_point lastSent{};
		while (!ShouldStop)
		{
			auto now = std::chrono::steady_clock::now();
			{
				std::shared_lock guard(Mutex);
				if (!InputPinId)
					continue;
				std::chrono::nanoseconds diff = now - lastSent;
				if (diff.count() < DeltaNanosec)
					continue;
			}
			nosEngine.SetPinDirty(*InputPinId);
			lastSent = now;
		}
	}

	uint64_t DeltaNanosec = 0;
	std::optional<uuid> InputPinId = std::nullopt;
	std::shared_mutex Mutex;
	std::atomic_bool ShouldStop;
	std::jthread Thread;
};

nosResult RegisterFrameInterpolator(nosNodeFunctions* nodeFunctions)
{
	NOS_BIND_NODE_CLASS(NSN_ClassName_FrameInterpolator, FrameInterpolatorNode, nodeFunctions)


		fs::path root = nosEngine.Plugin->RootFolderPath;
	auto basicInterpPath = (root / "." / "Shaders" / "BasicInterpolation.frag").generic_string();

	const std::vector<std::pair<Name, std::tuple<nosShaderStage, const char*>>> shaders = {
		{NSN_FrameInterpolator_BasicInterpolationShader, { NOS_SHADER_STAGE_FRAG, basicInterpPath.c_str()}}
	};

	std::vector<nosShaderInfo> shaderInfos;
	for (auto& [name, data] : shaders)
	{
		auto& [stage, path] = data;
		shaderInfos.push_back(nosShaderInfo{
			.ShaderName = name,
			.Source = {
				.Stage = stage,
				.GLSLPath = path,
			},
			.AssociatedNodeClassName = NSN_ClassName_FrameInterpolator,
		});
	}
	auto ret = nosVulkan->RegisterShaders(shaderInfos.size(), shaderInfos.data());
	if (NOS_RESULT_SUCCESS != ret)
		return ret;
	std::vector<nosPassInfo> passes =
	{
		{.Key = NSN_FrameInterpolator_BasicInterpolationPass, .Shader = NSN_FrameInterpolator_BasicInterpolationShader, .MultiSample = 1},
	};
	ret = nosVulkan->RegisterPasses(passes.size(), passes.data());
	if (NOS_RESULT_SUCCESS != ret)
		return ret;
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::test