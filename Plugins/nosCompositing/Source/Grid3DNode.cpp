// Copyright MediaZ Teknoloji A.S. All Rights Reserved.


#include <Nodos/Plugin.hpp>
#include <nosTrack/Track_generated.h>
#include <nosSysVulkan/Helpers.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "Names.h"

namespace nos::compositing
{
NOS_REGISTER_NAME(Track);
NOS_REGISTER_NAME(GridColor);
NOS_REGISTER_NAME(Step);
NOS_REGISTER_NAME(MVP);
NOS_REGISTER_NAME(GRID_3D_PASS);

glm::mat4 MakeView(glm::vec3 pos, glm::vec3 rot)
{
	rot = glm::radians(rot);
	auto mat = (glm::mat3)glm::eulerAngleZYX(rot.z, -rot.y, -rot.x);
	return glm::lookAtLH(pos, pos + mat[0], mat[2]);
}

glm::mat4 Perspective(f32 fovx, f32 pixelAspectRatio, glm::vec2 sensorSize, glm::vec2 centerShift)
{
	if (glm::vec2(0) == sensorSize)
	{
		sensorSize = glm::vec2(1);
		centerShift = glm::vec2(0);
	}

	const f32 near = 0.1f;
	const f32 far = 10000.0f;
	const f32 X = 1.f / tanf(glm::radians(fovx * 0.5f));
	const f32 Y = -X * (sensorSize.x / sensorSize.y) * pixelAspectRatio;
	const auto S = -centerShift / sensorSize;
	const f32 Z = far / (far - near);
	return glm::mat4(
		glm::vec4(X, 0, 0, 0),
		glm::vec4(0, Y, 0, 0),
		glm::vec4(S.x, -S.y, Z, 1.0f),
		glm::vec4(0, 0, -near * Z, 0));
}

void UpdateVertexBuffer(
	nosVertexData& data, 
	nos::ObjectRef& bufferObject,
	const void* verticesData,
	size_t verticesSize,
	const void* indicesData,
	size_t indicesSize,
	size_t indicesCount,
	const char* tag)
{
	uint32_t bufferSize = (uint32_t)(verticesSize + indicesSize);

	auto info = nos::sys::vulkan::GetResourceInfo(bufferObject).value_or({});
	if (info.Buffer.Size < bufferSize)
	{
		bufferObject = {};
		info = {};
		info.Type = NOS_RESOURCE_TYPE_BUFFER;
		info.Buffer.Size = bufferSize;
		info.Buffer.Usage = nosBufferUsage(NOS_BUFFER_USAGE_VERTEX_BUFFER | NOS_BUFFER_USAGE_INDEX_BUFFER);
		info.Buffer.MemoryFlags = NOS_MEMORY_FLAGS_HOST_VISIBLE;
		nosVulkan->CreateResource(&info, 0, tag, &bufferObject.GetStorage());
		data.Buffer = bufferObject;
	}

	data.VertexOffset = 0;
	data.IndexOffset = (uint32_t)verticesSize;
	data.IndexCount = (uint32_t)indicesCount;
	
	u8* mapping = nosVulkan->Map(bufferObject);
	if (mapping)
	{
		memcpy(mapping, verticesData, verticesSize);
		memcpy(mapping + verticesSize, indicesData, indicesSize);
	}
}
struct Grid3DNode : public NodeContext
{
public:

	nosVertexData VertexData = {};
	nos::ObjectRef BufferObject = {};

	nosResult OnCreate(nosFbNodePtr node) override
	{
		glm::vec3 vertices[] = {
			{-10000, -10000, 0},
			{ 10000, -10000, 0},
			{-10000,  10000, 0},
			{ 10000,  10000, 0}
		};

		std::vector<glm::uvec3> indices;
		indices.push_back(glm::uvec3(0, 1, 2));
		indices.push_back(glm::uvec3(3, 2, 1));

		indices.push_back(glm::uvec3(2, 1, 0));
		indices.push_back(glm::uvec3(1, 2, 3));

		UpdateVertexBuffer(VertexData, BufferObject, vertices, 8 * sizeof(glm::vec3), indices.data(), (u32)indices.size() * (u32)sizeof(glm::uvec3), (u32)indices.size() * 3, "Grid3DNode_VertexData");
		return NOS_RESULT_SUCCESS;
	}

	virtual nosResult ExecuteNode(nos::NodeExecuteParams const& params) override
	{
		// std::unordered_map<Name, void*> values = GetPinValues(params);
	
		
		auto track = params.GetPinData<nos::track::TTrack>(NSN_Track);
		auto gridColor = params.GetPinData<fb::vec4>(NSN_GridColor);
		auto step = params.GetPinData<float>(NSN_Step);

		glm::vec3 pos = reinterpret_cast<const glm::vec3&>(track.location);
		glm::vec3 rot = reinterpret_cast<const glm::vec3&>(track.rotation);

		auto& distortion = track.lens_distortion;
		auto MVP = Perspective(track.fov, track.pixel_aspect_ratio, reinterpret_cast<const glm::vec2&>(track.sensor_size), reinterpret_cast<const glm::vec2&>(distortion.center_shift())) *
				   MakeView(pos, rot);

		nosCmd cmd;
		nosCmdBeginParams bp = {.Name = nos::Name("Grid3D"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd};
		nosVulkan->Begin(&bp);

		nosRunPass2Params pass{};
		pass.Wireframe = false;
		pass.Key = NSN_GRID_3D_PASS;
		pass.Output = params.GetPinObject(NSN_Out);
		pass.DoNotClear = false;
		pass.ClearCol = nosVec4(0, 0, 0, 0);

		std::vector bindings = {
			nos::sys::vulkan::ShaderDataBinding(NSN_MVP, MVP),
			nos::sys::vulkan::ShaderDataBinding(NSN_GridColor, gridColor),
			nos::sys::vulkan::ShaderDataBinding(NSN_Step, step)
		};

		nosDrawCall drawCall;
		drawCall.Vertices = VertexData;
		drawCall.Bindings = bindings.data();
		drawCall.BindingCount = (u32)bindings.size();

		pass.DrawCalls = &drawCall;
		pass.DrawCallCount = 1;

		nosVulkan->RunPass2(cmd, &pass);
		nosVulkan->End(cmd, 0);

		return NOS_RESULT_SUCCESS;
	}

};

void RegisterGrid3D(nosNodeFunctions* nodeFunctions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("Grid3D"), Grid3DNode, nodeFunctions);
}

}
