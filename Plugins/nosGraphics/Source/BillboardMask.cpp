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
NOS_REGISTER_NAME(InDepth)
NOS_REGISTER_NAME(ReadInDepth)
NOS_REGISTER_NAME(ShadowDepth)
NOS_REGISTER_NAME(GroundLevel)

NOS_REGISTER_NAME(BillboardMask_Pass)
NOS_REGISTER_NAME(ShadowMask_Pass)

struct BillboardMask : NodeContext
{
	using NodeContext::NodeContext;

	enum class StatusMessageType
	{
		FailedToCreateRenderTarget = 1,
		FailedToCreateDepthBuffer = 2,
		InvalidInputDepth = 3,
	};

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value) override
	{
		if (pinName == NSN_ReadInDepth)
		{
			bool readInDepth = *nos::InterpretPinValue<bool>(value);
			if (readInDepth)
			{
				SetPinOrphanState(NSN_InDepth, nos::fb::PinOrphanStateType::ACTIVE);
			}
			else
			{
				SetPinOrphanState(NSN_InDepth, nos::fb::PinOrphanStateType::ORPHAN, "Read In Depth is disabled");
			}
		}
	}

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
		bool readInDepth = *pins.GetPinData<bool>(NSN_ReadInDepth);
		if (readInDepth)
		{
			auto inDepth = vkss::DeserializeTextureInfo(pins[NSN_InDepth].Data->Data);
			if (nosVulkan->IsStockTexture(&inDepth, nullptr) || 
				inDepth.Info.Texture.Width != resolution.x ||
				inDepth.Info.Texture.Height != resolution.y || inDepth.Info.Texture.Format != NOS_FORMAT_D32_SFLOAT)
			{
				SetOrAddStatusMessage(StatusMessageType::InvalidInputDepth,
									  nos::fb::TNodeStatusMessage{
										  .text = "Read In Depth is enabled, but InDepth is not a valid D32 depth texture matching Resolution.",
										  .type = nos::fb::NodeStatusMessageType::FAILURE,
									  });
				nosEngine.LogW(
					"Billboard Mask node is set to read input depth, but the provided texture is not a valid depth "
					"texture. Please provide a valid depth texture.");
				return NOS_RESULT_FAILED;
			}
			else
			{
				RemoveStatusMessage(StatusMessageType::InvalidInputDepth);
				RemoveStatusMessage(StatusMessageType::FailedToCreateDepthBuffer);
				SetPinValue(NSN_OutDepth, *pins[NSN_InDepth].Data);
				depth = inDepth;
			}
		}
		else
		{
			RemoveStatusMessage(StatusMessageType::InvalidInputDepth);
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
		}

		if (rt.Memory.Handle == 0 || depth.Memory.Handle == 0)
		{
			return NOS_RESULT_FAILED;
		}

		glm::mat4 viewMat = reinterpret_cast<glm::mat4&>(view.view);
		glm::mat4 projMat = reinterpret_cast<glm::mat4&>(view.left_handed_projection_matrix);
		glm::mat4 viewProjMat = projMat * viewMat;
		glm::mat4 invViewProjMat = glm::inverse(viewProjMat);

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
		auto cmd = vkss::BeginCmd(NOS_NAME("Billboard Mask"), NodeId);
		{

			glm::mat4 billboardMat = glm::scale(modelMat, glm::vec3(size.x, 1.0f, size.y));
			glm::mat4 billboardMvp = viewProjMat * billboardMat;

			auto bindings = std::array{vkss::ShaderBinding(NOS_NAME("MVP"), billboardMvp)};

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
				.DepthAttachment = nosDepthAttachment{.DepthBuffer = depth, .DoNotClear = readInDepth, .ClearValue = 1.0f}};
			nosVulkan->RunPass2(cmd, &passParams);
		}

		float shadowDepth = *pins.GetPinData<float>(NSN_ShadowDepth);
		if (shadowDepth > 0.0f)
		{
			float groundLevel = *pins.GetPinData<float>(NSN_GroundLevel);
			auto shadowMat = glm::scale(modelMat, glm::vec3(size.x, 1.0f, shadowDepth));
			glm::mat4 shadowMvp = viewProjMat * shadowMat;

			auto bindings = std::array{vkss::ShaderBinding(NOS_NAME("MVP"), shadowMvp),
									   vkss::ShaderBinding(NOS_NAME("InvViewProj"), invViewProjMat),
									   vkss::ShaderBinding(NOS_NAME("GroundLevel"), groundLevel),
									   vkss::ShaderBinding(NOS_NAME("DepthTexture"), depth)};
			nosDrawCall shadowDrawCall{.Bindings = bindings.data(),
									 .BindingCount = bindings.size(),
									 .Vertices = nosVertexData{
										 .IndexCount = 6,
										 .DepthFunc = NOS_DEPTH_FUNCTION_ALWAYS,
										 .DepthWrite = false,
										 .DepthTest = false,
									 }};

			nosRunPass2Params shadowPassParams{
				.Key = NSN_ShadowMask_Pass,
				.Output = rt,
				.DrawCalls = &shadowDrawCall,
				.DrawCallCount = 1,
				.Wireframe = NOS_FALSE,
				.Benchmark = 0,
				.DoNotClear = true,
				.ClearCol = {0.f, 0.f, 0.f, 0.f}};
			nosVulkan->RunPass2(cmd, &shadowPassParams);
		}

		nosVulkan->End(cmd, nullptr);
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterBillboardMask(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_BillboardMask, BillboardMask, fn);

	fs::path root = nosEngine.Module->RootFolderPath;
	auto billboardVertPath = (root / "Shaders" / "BillboardMask.vert").generic_string();
	auto billboardFragPath = (root / "Shaders" / "BillboardMask.frag").generic_string();
	auto shadowVertPath = (root / "Shaders" / "ShadowMask.vert").generic_string();
	auto shadowFragPath = (root / "Shaders" / "ShadowMask.frag").generic_string();

	// Register shaders
	auto billboardMask_Frag = NOS_NAME("BillboardMask_Frag");
	auto shadowMask_Frag = NOS_NAME("ShadowMask_Frag");
	auto billboardMask_Vert = NOS_NAME("BillboardMask_Vert");
	auto shadowMask_Vert = NOS_NAME("ShadowMask_Vert");
	auto shaders =
		std::array{nosShaderInfo{.ShaderName = billboardMask_Frag,
								 .Source = {.Stage = NOS_SHADER_STAGE_FRAG, .GLSLPath = billboardFragPath.c_str()},
								 .AssociatedNodeClassName = NSN_BillboardMask},
				   nosShaderInfo{.ShaderName = shadowMask_Frag,
								 .Source = {.Stage = NOS_SHADER_STAGE_FRAG, .GLSLPath = shadowFragPath.c_str()},
								 .AssociatedNodeClassName = NSN_BillboardMask},
				   nosShaderInfo{.ShaderName = billboardMask_Vert,
								 .Source = {.Stage = NOS_SHADER_STAGE_VERT, .GLSLPath = billboardVertPath.c_str()},
								 .AssociatedNodeClassName = NSN_BillboardMask},
				   nosShaderInfo{.ShaderName = shadowMask_Vert,
								 .Source = {.Stage = NOS_SHADER_STAGE_VERT, .GLSLPath = shadowVertPath.c_str()},
								 .AssociatedNodeClassName = NSN_BillboardMask}};
	auto ret = nosVulkan->RegisterShaders(shaders.size(), shaders.data());
	if (NOS_RESULT_SUCCESS != ret)
		return ret;

	auto passes = std::array<nosPassInfo, 2>{nosPassInfo{
												 .Key = NSN_BillboardMask_Pass,
												 .Shader = billboardMask_Frag,
												 .VertexShader = billboardMask_Vert,
												 .MultiSample = 1,
												 .Blend = NOS_BLEND_MODE_ALPHA_BLENDING,
											 },
											 nosPassInfo{
												 .Key = NSN_ShadowMask_Pass,
												 .Shader = shadowMask_Frag,
												 .VertexShader = shadowMask_Vert,
												 .MultiSample = 1,
												 .Blend = NOS_BLEND_MODE_ALPHA_BLENDING,
											 }};
	ret = nosVulkan->RegisterPasses(passes.size(), passes.data());
	if (NOS_RESULT_SUCCESS != ret)
		return ret;

	return NOS_RESULT_SUCCESS;
}

} // namespace nos::graphics
