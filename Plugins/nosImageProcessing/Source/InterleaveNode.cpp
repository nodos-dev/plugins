// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
#include <Nodos/Plugin.hpp>
#include <nosSysVulkan/Helpers.hpp>

#include "Names.h"

namespace nos::imageprocessing
{
NOS_REGISTER_NAME(Odd); // nos.fb.Texture node input pin
NOS_REGISTER_NAME(Even); // nos.fb.Texture node input pin

// shader and pass
NOS_REGISTER_NAME(INTERLEAVE_PASS);

struct InterleaveContext : public NodeContext
{
	nosResult ExecuteNode(nos::NodeExecuteParams const& params)
	{
		auto outTexture = params.GetPinObject(NSN_Out);
		auto outputTextureInfo = *nos::sys::vulkan::GetResourceInfo(outTexture);

		nosTextureFieldType fieldType;
		nosVulkan->GetResourceFieldType(outTexture, &fieldType);

		auto inputTexture = fieldType == nosTextureFieldType::NOS_TEXTURE_FIELD_TYPE_ODD
			                    ? params.GetPinObject(NSN_Odd)
			                    : params.GetPinObject(NSN_Even);

		nosCmd cmd;
		nosCmdBeginParams bp = {.Name = nos::Name("Render"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd};
		nosVulkan->Begin(&bp);

		nosRunPassParams interleave = {};
		interleave.Key = NSN_INTERLEAVE_PASS;
		std::vector bindings = {
			nos::sys::vulkan::ShaderTextureBinding(NSN_In, inputTexture, NOS_TEXTURE_FILTER_LINEAR)
		};
		interleave.Bindings = bindings.data();
		interleave.BindingCount = (u32)bindings.size();
		interleave.Output = outTexture;
		nosVulkan->RunPass(cmd, &interleave);
		nosVulkan->End(cmd, 0);

		return NOS_RESULT_SUCCESS;
	}
};

void RegisterInterleaveNode(nosNodeFunctions* nodeFunctions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("Interleave"), InterleaveContext, nodeFunctions);
}

} // namespace nos::imageprocessing
