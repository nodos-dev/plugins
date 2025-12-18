// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <Graphics_generated.h>
#include <nosVulkanSubsystem/Helpers.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace nos::graphics
{
NOS_REGISTER_NAME(BillboardMask)

NOS_REGISTER_NAME(Position)
NOS_REGISTER_NAME(Size)
NOS_REGISTER_NAME(RenderView)
NOS_REGISTER_NAME(Resolution)
NOS_REGISTER_NAME(OutRenderTarget)
NOS_REGISTER_NAME(OutDepth)

NOS_REGISTER_NAME(BillboardMask_Pass)

struct BillboardMask : NodeContext
{
	using NodeContext::NodeContext;

	enum class StatusMessageType
	{
		FailedToCreateRenderTarget = 1,
		FailedToCreateDepthBuffer = 2,
	};

	std::unordered_map<StatusMessageType, nos::fb::TNodeStatusMessage> ActiveStatusMessages;

	void SerializeAndSendStatusMessages()
	{
		if (ActiveStatusMessages.size() == 0)
		{
			ClearNodeStatusMessages();
			return;
		}
		std::vector<nos::fb::TNodeStatusMessage> messages;
		messages.reserve(ActiveStatusMessages.size());
		for (const auto& [type, statusMessage] : ActiveStatusMessages)
		{
			messages.push_back(statusMessage);
		}
		SetNodeStatusMessages(messages);
	}

	void SetOrAddStatusMessage(StatusMessageType msgType, nos::fb::TNodeStatusMessage message)
	{
		if (auto it = ActiveStatusMessages.find(msgType); it != ActiveStatusMessages.end())
		{
			if (it->second.text == message.text && it->second.type == message.type)
				return;
		}
		ActiveStatusMessages[msgType] = std::move(message);
		SerializeAndSendStatusMessages();
	}

	void RemoveStatusMessage(StatusMessageType msgType)
	{
		if (ActiveStatusMessages.erase(msgType) > 0)
			SerializeAndSendStatusMessages();
	}

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		NodeExecuteParams pins(params);
		TRenderView view = pins.GetPinData<TRenderView>(NSN_RenderView);
		glm::vec2 size = *pins.GetPinData<glm::vec2>(NSN_Size);
		glm::vec3 position = *pins.GetPinData<glm::vec3>(NSN_Position);

		nosVec2u resolution = *pins.GetPinData<nosVec2u>(NSN_Resolution);

		nosResourceShareInfo rt = vkss::DeserializeTextureInfo(pins[NSN_OutRenderTarget].Data->Data);

		if (rt.Info.Texture.Width != resolution.x || rt.Info.Texture.Height != resolution.y)
		{
			auto newRt = vkss::Resource::Create(
				nosTextureInfo{.Width = resolution.x,
							   .Height = resolution.y,
							   .Format = NOS_FORMAT_R8_UNORM,
							   .Filter = NOS_TEXTURE_FILTER_LINEAR,
							   .Usage = nosImageUsage(NOS_IMAGE_USAGE_SAMPLED | NOS_IMAGE_USAGE_RENDER_TARGET)},
				"Billboard Mask Output");
			if (!newRt)
			{
				SetOrAddStatusMessage(StatusMessageType::FailedToCreateRenderTarget,
									  nos::fb::TNodeStatusMessage{
										  .text = "Failed to create render target for Billboard Mask node.",
										  .type =
											  nos::fb::NodeStatusMessageType::FAILURE,
									  });
				rt = {};
			}
			else
			{
				SetPinValue(NSN_OutRenderTarget, newRt->ToPinData());
				rt = nosResourceShareInfo(*newRt);
				RemoveStatusMessage(StatusMessageType::FailedToCreateRenderTarget);
			}
		}

		nosResourceShareInfo depth = vkss::DeserializeTextureInfo(pins[NSN_OutDepth].Data->Data);
		if (depth.Info.Texture.Width != resolution.x || depth.Info.Texture.Height != resolution.y ||
			depth.Info.Texture.Format != NOS_FORMAT_D32_SFLOAT)
		{
			auto newDepth = vkss::Resource::Create(
				nosTextureInfo{.Width = resolution.x,
							   .Height = resolution.y,
							   .Format = NOS_FORMAT_D32_SFLOAT,
							   .Filter = NOS_TEXTURE_FILTER_LINEAR,
							   .Usage = nosImageUsage(NOS_IMAGE_USAGE_SAMPLED | NOS_IMAGE_USAGE_DEPTH_STENCIL)},
				"Billboard Mask Depth");
			if (!newDepth)
			{
				SetOrAddStatusMessage(StatusMessageType::FailedToCreateDepthBuffer,
									  nos::fb::TNodeStatusMessage{
										  .text = "Failed to create depth buffer for Billboard Mask node.",
										  .type = nos::fb::NodeStatusMessageType::FAILURE,
									  });
				depth = {};
			}
			else
			{
				SetPinValue(NSN_OutDepth, newDepth->ToPinData());
				depth = nosResourceShareInfo(*newDepth);
				RemoveStatusMessage(StatusMessageType::FailedToCreateDepthBuffer);
			}
		}

		if (rt.Memory.Handle == 0 || depth.Memory.Handle == 0)
		{
			return NOS_RESULT_FAILED;
		}

		glm::mat4 viewMat = reinterpret_cast<glm::mat4&>(view.view);
		glm::mat4 projMat = reinterpret_cast<glm::mat4&>(view.left_handed_projection_matrix);

		glm::mat4 modelMat = glm::mat4(1.0f);
		// Face the camera
		glm::vec3 cameraPos = glm::vec3(glm::inverse(viewMat)[3]);
		cameraPos.z = position.z;
		glm::vec3 toCamera = glm::normalize(cameraPos - position);
		glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
		glm::vec3 right = glm::normalize(glm::cross(toCamera, up));
		modelMat = glm::translate(modelMat, position);
		modelMat[0] = glm::vec4(right, 0.0f);
		modelMat[1] = glm::vec4(toCamera, 0.0f);
		modelMat[2] = glm::vec4(up, 0.0f);
		// Translation writes to the 4th column, so no need to set it again
		modelMat = glm::scale(modelMat, glm::vec3(size.x, 1.0f, size.y));

		glm::mat4 mvp = projMat * viewMat * modelMat;

		auto bindings = std::array{vkss::ShaderBinding(NOS_NAME("MVP"), mvp)};

		nosDrawCall drawCall{.Bindings = bindings.data(),
							 .BindingCount = bindings.size(),
							 .Vertices = nosVertexData{
								 .IndexCount = 6,
								 .DepthFunc = NOS_DEPTH_FUNCTION_LESS,
								 .DepthWrite = true,
								 .DepthTest = true,
							 }};

		nosRunPass2Params passParams{
			.Key = NSN_BillboardMask_Pass,
			.Output = rt,
			.DrawCalls = &drawCall,
			.DrawCallCount = 1,
			.Wireframe = NOS_FALSE,
			.Benchmark = 0,
			.DoNotClear = false,
			.ClearCol = {0.f, 0.f, 0.f, 0.f},
			.DepthAttachment = nosDepthAttachment{.DepthBuffer = depth, .DoNotClear = false, .ClearValue = 1.0f}};

		auto cmd = vkss::BeginCmd(NOS_NAME("Billboard Mask"), NodeId);
		nosVulkan->RunPass2(cmd, &passParams);
		nosVulkan->End(cmd, nullptr);
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterBillboardMask(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_BillboardMask, BillboardMask, fn);

	fs::path root = nosEngine.Module->RootFolderPath;
	auto vertPath = (root / "Shaders" / "BillboardMask.vert").generic_string();
	auto fragPath = (root / "Shaders" / "BillboardMask.frag").generic_string();

	// Register shaders
	auto billboardMask_Frag = NOS_NAME("BillboardMask_Frag");
	auto billboardMask_Vert = NOS_NAME("BillboardMask_Vert");
	std::array<nosShaderInfo, 2> shaders = {
		nosShaderInfo{.ShaderName = billboardMask_Frag,
					  .Source = {.Stage = NOS_SHADER_STAGE_FRAG, .GLSLPath = fragPath.c_str()},
					  .AssociatedNodeClassName = NSN_BillboardMask},
		nosShaderInfo{.ShaderName = billboardMask_Vert,
					  .Source = {.Stage = NOS_SHADER_STAGE_VERT, .GLSLPath = vertPath.c_str()},
					  .AssociatedNodeClassName = NSN_BillboardMask}};
	auto ret = nosVulkan->RegisterShaders(shaders.size(), shaders.data());
	if (NOS_RESULT_SUCCESS != ret)
		return ret;

	nosPassInfo pass = {
		.Key = NSN_BillboardMask_Pass,
		.Shader = billboardMask_Frag,
		.VertexShader = billboardMask_Vert,
		.MultiSample = 1,
		.Blend = NOS_BLEND_MODE_ALPHA_BLENDING,
	};
	ret = nosVulkan->RegisterPasses(1, &pass);
	if (NOS_RESULT_SUCCESS != ret)
		return ret;

	return NOS_RESULT_SUCCESS;
}

} // namespace nos::graphics
