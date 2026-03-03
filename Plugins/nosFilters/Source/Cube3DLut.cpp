#include <Nodos/Plugin.hpp>
#include <fstream>

#include <nosSysVulkan/Helpers.hpp>

#include <glm/vec3.hpp>

#include "Names.h"

namespace nos::filters
{


struct Cube3DLUTContext : NodeContext
{
	using NodeContext::NodeContext;

	enum : u32
	{
		NONE = 0,
		LUT_1D = 1,
		LUT_3D = 3,
	} type;

	f32 DomainScale = 1.f;
	f32 DomainOffset = 0.f;
	glm::vec3 RangeScale = glm::vec3(1.f);
	glm::vec3 RangeOffset = glm::vec3(0.f);

	uint32_t Size = 0;
	nos::ObjectRef SSBO = {};
	std::string LoadedLutName = "";

	std::vector<float> LUT;

	std::unordered_map<std::string, std::string> Properties;

	std::string NodeStatus;

	void UpdateNodeStatus(const std::string& status, fb::NodeStatusMessageType type)
	{

		if (NodeStatus == status)
			return;

		NodeStatus = status;

		std::vector<flatbuffers::Offset<nos::fb::NodeStatusMessage>> msg;
		flatbuffers::FlatBufferBuilder fbb;
		msg.push_back(fb::CreateNodeStatusMessageDirect(fbb, NodeStatus.c_str(), type));

		HandleEvent(CreateAppEvent(fbb,
		                           nos::CreatePartialNodeUpdateDirect(fbb,
		                                                              &NodeId,
		                                                              nos::ClearFlags::NONE,
		                                                              0,
		                                                              0,
		                                                              0,
		                                                              0,
		                                                              0,
		                                                              0,
		                                                              &msg)));
	}


	nosResult ExecuteNode(nos::NodeExecuteParams const& params) override
	{
		if (!SSBO)
		{
			auto cmd = nos::sys::vulkan::BeginCmd(NOS_NAME("No Op"), NodeId);

			nosVulkan->Copy(cmd, params.GetPinObject(NSN_In), params.GetPinObject(NSN_Out), nullptr);
			nosVulkan->End(cmd, 0);

			std::string status = "No LUT Loaded";
			UpdateNodeStatus(status, fb::NodeStatusMessageType::WARNING);
		}
		else
		{
			std::string status = LoadedLutName;
			UpdateNodeStatus(status, fb::NodeStatusMessageType::INFO);

			f32 domainGamma = *params.GetPinValue<f32>(nos::Name("InputGamma"));
			f32 rangeGamma = *params.GetPinValue<f32>(nos::Name("OutputGamma"));

			std::vector<nosShaderBinding> bindings = {
				nos::sys::vulkan::ShaderTextureBinding(NSN_In, params.GetPinObject(NSN_In), NOS_TEXTURE_FILTER_LINEAR),
				nos::sys::vulkan::ShaderTextureBinding(NSN_Out,
				                                       params.GetPinObject(NSN_Out),
				                                       NOS_TEXTURE_FILTER_LINEAR),
				nos::sys::vulkan::ShaderDataBinding(nos::Name("Size"), Size),
				nos::sys::vulkan::ShaderDataBinding(nos::Name("Dim"), type),
				nos::sys::vulkan::ShaderDataBinding(nos::Name("LUT"), SSBO),
				nos::sys::vulkan::ShaderDataBinding(nos::Name("DomainScale"), DomainScale),
				nos::sys::vulkan::ShaderDataBinding(nos::Name("DomainOffset"), DomainOffset),
				nos::sys::vulkan::ShaderDataBinding(nos::Name("DomainGamma"), domainGamma),
				nos::sys::vulkan::ShaderDataBinding(nos::Name("RangeGamma"), rangeGamma),
				nos::sys::vulkan::ShaderDataBinding(nos::Name("RangeScale"), RangeScale),
				nos::sys::vulkan::ShaderDataBinding(nos::Name("RangeOffset"), RangeOffset),
			};

			auto in = *nos::sys::vulkan::GetResourceInfo(params.GetPinObject(NSN_In));

			u32 sz = (u32)std::ceil(std::sqrt(in.Texture.Width * in.Texture.Height) / 64);

			nosRunComputePassParams colorMapperPass = {
				.Key = nos::Name("COLOR_MAPPER_PASS"),
				.Bindings = bindings.data(),
				.BindingCount = (u32)bindings.size(),
				.DispatchSize = {sz, sz},
			};
			auto cmd = nos::sys::vulkan::BeginCmd(NOS_NAME("Color Map"), NodeId);

			nosVulkan->RunComputePass(cmd, &colorMapperPass);
			nosVulkan->End(cmd, 0);
		}
		return NOS_RESULT_SUCCESS;
	}

	nosResult LoadLUTFile(const char* path)
	{
		Properties = {};

		std::istringstream ss(ReadToString(path));

		std::string line;
		std::vector<float> data;

		float domainMin = 0.f;
		float domainMax = 1.f;

		glm::vec3 rangeMin = glm::vec3(+INFINITY);
		glm::vec3 rangeMax = glm::vec3(-INFINITY);

		while (std::getline(ss, line))
		{
			if (line.empty() || line.starts_with("#"))
				continue;
			std::istringstream ls(line);
			std::string dump;
			if (line.starts_with("LUT_3D_SIZE"))
			{
				type = LUT_3D;
				ls >> dump >> Size;
			}
			if (line.starts_with("LUT_1D_SIZE"))
			{
				type = LUT_1D;
				ls >> dump >> Size;
			}

			if (line.starts_with("LUT_3D_INPUT_RANGE"))
			{
				if (LUT_1D == type)
				{
					//TODO: log err
				}
				ls >> dump >> domainMin >> domainMax;
			}
			float x;

			int i = 0;

			while ((ls >> x))
			{
				rangeMin[i] = std::min(rangeMin[i], x);
				rangeMax[i] = std::max(rangeMax[i], x);
				data.push_back(x);
				++i %= 3;
			}
		}

		DomainScale = domainMax - domainMin;
		DomainOffset = domainMin;

		RangeScale = rangeMax - rangeMin;
		RangeOffset = rangeMin;

		{
			SSBO = {};
			nosResourceInfo info = {
				.Type = NOS_RESOURCE_TYPE_BUFFER,
				.Buffer = {
					.Size = uint32_t(data.size() * sizeof(data[0])),
					.Usage = NOS_BUFFER_USAGE_STORAGE_BUFFER,
					.MemoryFlags = NOS_MEMORY_FLAGS_HOST_VISIBLE
				}
			};
			nosVulkan->CreateResource(&info, 0, "Cube3DLut_ssbo", &SSBO.GetStorage());
		}

		auto mapping = nosVulkan->Map(SSBO);
		memcpy(mapping, data.data(), data.size() * sizeof(data[0]));
		LoadedLutName = path;
		return NOS_RESULT_SUCCESS;
	}

	static nosResult GetFunctions(size_t* outCount, nosName* outFunctionNames, nosPfnNodeFunctionExecute* outFunction)
	{
		*outCount = 1;
		if (!outFunctionNames || !outFunction)
			return NOS_RESULT_SUCCESS;
		*outFunctionNames = NOS_NAME_STATIC("Load LUT File");
		*outFunction = [](void* ctx, nosFunctionExecuteParams* params) {
			const char* path = nos::NodeExecuteParams(params->FunctionNodeExecuteParams).GetPinValue<const char*>(
				nos::Name("LUT File"));
			return ((Cube3DLUTContext*)ctx)->LoadLUTFile(path);
		};
		return NOS_RESULT_SUCCESS;
	}
};

void RegisterCube3DLUTNode(nosNodeFunctions* nodeFunctions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("Cube3DLUT"), Cube3DLUTContext, nodeFunctions);
}

}