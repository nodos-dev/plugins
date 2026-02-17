// Copyright Zero Density AS. All Rights Reserved.

#include "Common.h"

namespace nos::compositing
{

using glm::vec2;
using glm::uvec2;
using glm::ivec2;
using glm::vec3;
using glm::uvec3;
using glm::mat4;

NOS_REGISTER_NAME(OutputResolution);

static nos::Name NSN_Inputs(int id)
{
	static nos::Name names[4] =
	{
		NOS_NAME("Input1"),
		NOS_NAME("Input2"),
		NOS_NAME("Input3"),
		NOS_NAME("Input4")
	};
	return names[id];
}

static nos::Name NSN_TextureResolutions(int id)
{
	static nos::Name names[4] =
	{
		NOS_NAME("Texture1Resolution"),
		NOS_NAME("Texture2Resolution"),
		NOS_NAME("Texture3Resolution"),
		NOS_NAME("Texture4Resolution")
	};
	return names[id];
}

static nos::Name NSN_TexturePans(int id)
{
	static nos::Name names[4] =
	{
		NOS_NAME("Texture1Pan"),
		NOS_NAME("Texture2Pan"),
		NOS_NAME("Texture3Pan"),
		NOS_NAME("Texture4Pan")
	};
	return names[id];
}

static nos::Name NSN_TextureRotations(int id)
{
	static nos::Name names[4] =
	{
		NOS_NAME("Texture1Rotation"),
		NOS_NAME("Texture2Rotation"),
		NOS_NAME("Texture3Rotation"),
		NOS_NAME("Texture4Rotation")
	};
	return names[id];
}; // float

NOS_REGISTER_NAME(TEXTURE_MAPPER_PASS);
NOS_REGISTER_NAME(TEXTURE_MAPPER_ALPHA_BLEND_PASS);
NOS_REGISTER_NAME(MVP);
NOS_REGISTER_NAME(InputTexture);

struct TextureMapperContext : public NodeContext
{
private:
	nosVertexData VertexData = {};
	nos::ObjectRef BufferObject = {};

	struct TextureParams
	{
		bool connected = false;
		ivec2 resolution;
		ivec2 pan;
		float rotation;
	};

	TextureParams Params[4];

public:

	virtual void OnPinConnected(nos::Name pinName, const nos::uuid& connectedPin) override
	{
		for(int i=0; i<4; ++i)
			if (pinName == NSN_Inputs(i))
			{
				Params[i].connected = true;
				break;
			}
	}

	virtual void OnPinDisconnected(nos::Name pinName) override
	{
		for(int i=0; i<4; ++i)
			if (pinName == NSN_Inputs(i))
			{
				Params[i].connected = false;
				break;
			}
	}

	virtual nosResult ExecuteNode(nos::NodeExecuteParams const& params) override
	{
		CreateVertexData();

		for (int i = 0; i < 4; ++i)
		{
			Params[i].resolution = *params.GetPinData<ivec2>(NSN_TextureResolutions(i));
			Params[i].pan = *params.GetPinData<ivec2>(NSN_TexturePans(i));
			Params[i].rotation = *params.GetPinData<float>(NSN_TextureRotations(i));
		}

		uvec2 outResolution = *params.GetPinData<uvec2>(NSN_OutputResolution);

		auto outputTexture = params.GetPinObject(NSN_Output);

		if (RequestNewTextureSize(NSN_Output, outputTexture, outResolution))
			return NOS_RESULT_SUCCESS;

		nosCmd cmd;
		nosCmdBeginParams bp = {.Name = nos::Name("ExecuteNode"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd};
		nosVulkan->Begin(&bp);

		bool alphaBlend = *params.GetPinData<bool>(NOS_NAME_STATIC("AlphaBlend"));

		nosRunPass2Params pass{};
		pass.Wireframe = false;
		pass.Key = alphaBlend ? NOS_NAME_STATIC("TEXTURE_MAPPER_ALPHA_BLEND_PASS") : NOS_NAME_STATIC("TEXTURE_MAPPER_PASS");
		pass.Output = outputTexture;
		pass.DoNotClear = false;
		pass.ClearCol = nosVec4(0, 0, 0, 0);

		std::vector<nosDrawCall> drawCalls;

		mat4 MVPs[4];
		std::vector<nosShaderBinding> bindings[4];


		for(int i=0; i<4; ++i)
			if (Params[i].connected)
			{
				MVPs[i] = GenMVP(outResolution, Params[i]);
				bindings[i] = {
					nos::sys::vulkan::ShaderDataBinding(NSN_MVP, MVPs[i]),
					nos::sys::vulkan::ShaderTextureBinding(NSN_InputTexture, params.GetPinObject(NSN_Inputs(i)), NOS_TEXTURE_FILTER_LINEAR)
				};

				nosDrawCall drawCall;
				drawCall.Vertices = VertexData;
				drawCall.Bindings = bindings[i].data();
				drawCall.BindingCount = (u32)bindings[i].size();
				drawCalls.push_back(drawCall);
			}

		pass.DrawCalls = drawCalls.data();
		pass.DrawCallCount = (u32)drawCalls.size();

		nosVulkan->RunPass2(cmd, &pass);
		nosVulkan->End(cmd, 0);
		return NOS_RESULT_SUCCESS;
	}

private:


	mat4 GenMVP(const uvec2& outputResolution,
					 const TextureParams& params)
	{
		mat4 mvp = glm::identity<mat4>();

		mvp = glm::scale(mvp, 2.f / vec3(outputResolution, 1));

		vec2 offset = -glm::min(vec2(0, 0), vec2(params.resolution));
		mvp = glm::translate(mvp, vec3(vec2(params.pan) + offset - vec2 (outputResolution)/2.f, 0));

		mvp = glm::rotate(mvp, glm::radians(params.rotation), vec3(0, 0, 1));
		mvp = glm::scale(mvp, vec3(params.resolution, 1));

		return mvp;
	}

	void CreateVertexData()
	{
		if (VertexData.Buffer)
			return;

		const vec3 vertices[4] = {
			{0, 0, 0},
			{0, 1, 0},
			{1, 1, 0},
			{1, 0, 0},
		};

		const uvec3 indices[4] =
		{
			{0, 1, 3},
			{1, 2, 3},
			{3, 1, 0},
			{3, 2, 1}
		};

		const int vSize = sizeof(vertices);
		const int iSize = sizeof(indices);


		uint32_t bufferSize = vSize + iSize;

		nosResourceInfo info = {};

		info.Type = NOS_RESOURCE_TYPE_BUFFER;
		info.Buffer.Size = bufferSize;
		info.Buffer.Usage = nosBufferUsage(NOS_BUFFER_USAGE_VERTEX_BUFFER | NOS_BUFFER_USAGE_INDEX_BUFFER);
		info.Buffer.MemoryFlags = NOS_MEMORY_FLAGS_HOST_VISIBLE;

		VertexData.VertexOffset = 0;
		VertexData.IndexOffset = vSize;
		VertexData.IndexCount = 12;

		nosVulkan->CreateResource(&info, 0, "TextureMapper_VertexData.Buffer", &BufferObject.GetStorage());
		VertexData.Buffer = BufferObject;

		u8* mapping = nosVulkan->Map(VertexData.Buffer);
		if (mapping)
		{
			memcpy(mapping, vertices, vSize);
			memcpy(mapping + vSize, indices, iSize);
		}
	}


	bool RequestNewTextureSize(nosName name,
							   nosTextureObject& texture,
							   uvec2 requiredResolution)
		{
			auto info = *nos::sys::vulkan::GetResourceInfo(texture);

			if (info.Texture.Width == requiredResolution.x &&
				info.Texture.Height == requiredResolution.y)
				return false;

			nosEngine.LogI("Requesting texture %dx%d", requiredResolution.x, requiredResolution.y);
			info.Texture.Width = requiredResolution.x;
			info.Texture.Height = requiredResolution.y;

			nos::ObjectRef newTexture;
			nosVulkan->CreateResource(&info, 0, 0, &newTexture.GetStorage());
			nosEngine.SetPinObject(GetPinId(name).value(), newTexture);
			texture = newTexture;
			return true;
		}
	};

	void RegisterTextureMapperNode(nosNodeFunctions* nodeFunctions)
	{
		NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("TextureMapper"), TextureMapperContext, nodeFunctions);
	}

} // namespace nos::compositing

