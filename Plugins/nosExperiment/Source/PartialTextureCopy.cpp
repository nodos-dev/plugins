// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

#include <nosSysVulkan/Helpers.hpp>

namespace nos::experiment
{
NOS_REGISTER_NAME_SPACED(Nos_Utilities_PartialTextureCopy, "nos.test.PartialTextureCopy")

struct PartialTextureCopy : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		auto input = params.GetPinObject<sys::vulkan::Texture>(NOS_NAME("Input"));
		auto srcOffset = params.GetPinData<nos::fb::vec2i>(NOS_NAME("SrcOffset"));
		auto dstOffset = params.GetPinData<nos::fb::vec2i>(NOS_NAME("DstOffset"));
		auto size = params.GetPinData<nos::fb::vec2u>(NOS_NAME("Size"));
		auto output = params.GetPinObject<sys::vulkan::Texture>(NOS_NAME("Output"));

		if (!input || !output)
			return NOS_RESULT_INVALID_ARGUMENT;
		if (size->x() <= 0 || size->y() <= 0)
			return NOS_RESULT_INVALID_ARGUMENT;

		nosCopyParams copyParams;
		copyParams.RegionCount = 1;
		nosCopyRegion region{};
		region.TextureCopy.SrcOffset = { srcOffset->x(), srcOffset->y(), 0 };
		region.TextureCopy.DstOffset = { dstOffset->x(), dstOffset->y(), 0};
		region.TextureCopy.Extent = { size->x(), size->y(), 1 };
		copyParams.Regions = &region;

		nosCmd cmd{};
		nosCmdBeginParams beginParams{ .Name = NOS_NAME("Partial Texture Copy"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd };
		nosVulkan->Begin(&beginParams);
		nosVulkan->Clear(cmd, output, nosVec4{ 0, 0, 0, 1 });
		nosVulkan->Copy(cmd, input, output, &copyParams);
		nosVulkan->End(cmd, nullptr);
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterPartialTextureCopy(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_Nos_Utilities_PartialTextureCopy, PartialTextureCopy, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::test