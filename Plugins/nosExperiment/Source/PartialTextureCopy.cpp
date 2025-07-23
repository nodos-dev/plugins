// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

#include <nosVulkanSubsystem/Helpers.hpp>

namespace nos::experiment
{
NOS_REGISTER_NAME_SPACED(Nos_Utilities_PartialTextureCopy, "nos.test.PartialTextureCopy")

struct PartialTextureCopy : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		auto pins = GetPinValues(params);
		auto input = GetPinValue<sys::vulkan::Texture>(pins, NOS_NAME("Input"));
		auto srcOffset = GetPinValue<nos::fb::vec2i>(pins, NOS_NAME("SrcOffset"));
		auto dstOffset = GetPinValue<nos::fb::vec2i>(pins, NOS_NAME("DstOffset"));
		auto size = GetPinValue<nos::fb::vec2u>(pins, NOS_NAME("Size"));
		auto output = GetPinValue<sys::vulkan::Texture>(pins, NOS_NAME("Output"));

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
		auto src = vkss::DeserializeResourceFBInfo(*input);
		auto dst = vkss::DeserializeResourceFBInfo(*output);

		nosCmd cmd{};
		nosCmdBeginParams beginParams{ .Name = NOS_NAME("Partial Texture Copy"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd };
		nosVulkan->Begin(&beginParams);
		nosVulkan->Clear(cmd, &dst, nosVec4{ 0, 0, 0, 1 });
		nosVulkan->Copy(cmd, &src, &dst, &copyParams);
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