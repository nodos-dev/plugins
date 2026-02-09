// Copyright Zero Density AS. All Rights Reserved.
#include "Common.h"
#include "nosUtilities2/CanvasMapper_generated.h"

namespace nos
{
    struct CanvasMapperContext : public NodeContext
    {

		nosResult ExecuteNode(nos::NodeExecuteParams const& params)
		{
			//if (!HasPinValues(params, NOS_NAME_STATIC("Input"), NOS_NAME_STATIC("BackgroundColor"), NOS_NAME_STATIC("RGSS")))
			//	return NOS_RESULT_INVALID_ARGUMENT;

			// auto values = GetPinValues(params);

			auto arrayObj = nos::ArrayObjectRef(params.GetPinObject(NSN_Input));

			auto inputs = (flatbuffers::Vector<flatbuffers::Offset<nos::utilities2::CanvasLayer>>*)arrayObj.GetObjectDataView().Ok()->Data;

			if (0 == inputs->size())
				return NOS_RESULT_SUCCESS;

			auto output = *nos::sys::vulkan::GetResourceInfo(params.GetPinObject(NSN_Output));
			auto outputSize = glm::vec2(output.Texture.Width, output.Texture.Height);
			auto rgss = *params.GetPinData<bool>(NOS_NAME_STATIC("RGSS"));

			std::array<nos::fb::vec2, 16>  pos = {};
			std::array<nos::fb::vec2, 16>  sca = {};
			std::array<float, 16>  rot = {};
			std::array<nos::fb::vec2, 16>  ori = {};
			u32 ble = 0;
			std::array<float, 16>  opa = {};
			std::vector<nosTextureObject> textures;
			std::vector<nosTextureFilter> filters;

			u32 last = 0;

			for (u32 i = 0; i < inputs->size(); ++i)
			{
				auto layer = inputs->Get(i);
				if (!layer->texture())
					continue;

				auto texture = *arrayObj.GetElement<nos::CompositeObjectRef>(i)->GetField(NSN_Texture);
				textures.push_back(texture);
				filters.push_back(NOS_TEXTURE_FILTER_LINEAR);
				pos[last] = *layer->position();
				rot[last] = layer->rotation();
				ori[last] = *layer->origin();
				sca[last] = nos::fb::vec2(
					float(layer->size()->x()) / outputSize.x,
					float(layer->size()->y()) / outputSize.y
				);
				ble |= u32(layer->blend_mode()) << i;
				opa[last] = layer->opacity();
				last++;
			}

			u32 count = (u32)textures.size();
			if (0 == count)
				return NOS_RESULT_SUCCESS;

			auto backgroundColor = *params.GetPinData< nosVec4>(NOS_NAME_STATIC("BackgroundColor"));
			std::vector bindings = {
				nos::sys::vulkan::ShaderTextureArrayBinding(NOS_NAME_STATIC("Textures"),   textures.data(), filters.data(), (u32)textures.size()),
				nos::sys::vulkan::ShaderDataBinding(NOS_NAME_STATIC("OutputSize"), outputSize),
				nos::sys::vulkan::ShaderDataBinding(NOS_NAME_STATIC("BackgroundColor"), backgroundColor),
				nos::sys::vulkan::ShaderDataBinding(NOS_NAME_STATIC("Positions"),  pos),
				nos::sys::vulkan::ShaderDataBinding(NOS_NAME_STATIC("Scales"),     sca),
				nos::sys::vulkan::ShaderDataBinding(NOS_NAME_STATIC("Origins"),    ori),
				nos::sys::vulkan::ShaderDataBinding(NOS_NAME_STATIC("Rotations"),  rot),
				nos::sys::vulkan::ShaderDataBinding(NOS_NAME_STATIC("Opacities"),  opa),
				nos::sys::vulkan::ShaderDataBinding(NOS_NAME_STATIC("BlendModes"), ble),
				nos::sys::vulkan::ShaderDataBinding(NOS_NAME_STATIC("Texture_Count"), count),
				nos::sys::vulkan::ShaderDataBinding(NOS_NAME_STATIC("RGSS"), rgss),
			};

			nosCmd cmd;
			nosCmdBeginParams bp = { .Name = nos::Name("compositePass"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd };
			nosVulkan->Begin(&bp);
			nosRunPassParams compositePass = {};
			compositePass.Key = NOS_NAME_STATIC("CANVAS_MAPPER_PASS");
			compositePass.Bindings = bindings.data();
			compositePass.BindingCount = (u32)bindings.size();
			compositePass.Output = params.GetPinObject(NSN_Output);
			nosVulkan->RunPass(cmd, &compositePass);
			nosVulkan->End(cmd, 0);
			return NOS_RESULT_SUCCESS;
		}

	};


    void RegisterCanvasMapperNode(nosNodeFunctions* nodeFunctions)
    {
		NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("CanvasMapper"), CanvasMapperContext, nodeFunctions);
    }

} // namespace nos
