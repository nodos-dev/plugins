// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include "Layout_generated.h"

NOS_REGISTER_NAME(FreeLayout)
NOS_REGISTER_NAME(QuadLayout)
namespace nos::utilities
{
	nosResult NOSAPI_CALL ExecuteFreeLayout(void* _, nosNodeExecuteParams* params)
	{
		nos::NodeExecuteParams args(params);
		auto& outSize = *args.GetPinData<nos::fb::vec2u>(NOS_NAME("OutputSize"));
		auto items = args.GetPinData<flatbuffers::Vector<const layout::FreeLayoutItem*>>(NOS_NAME("Items"));

		layout::TLayoutDrawList drawList{};
		drawList.items.reserve(items->size());

		int textureId = 0;
		for (auto itemPtr : *items)
		{
			auto& item = *itemPtr;
			layout::LayoutTexturedQuadDrawItem drawItem{};
			drawItem.mutable_position() = nos::fb::vec2(item.position().x() / (float) outSize.x(), item.position().y() / (float) outSize.y());
			drawItem.mutable_size() = nos::fb::vec2(item.size().x() / (float)outSize.x(), item.size().y() / (float)outSize.y());
			drawItem.mutate_texture_id(textureId);
			drawList.items.push_back(std::move(drawItem));
			textureId++;
		}
		nosEngine.SetPinValueByName(args.NodeId, NOS_NAME("OutLayout"), nos::Buffer::From(drawList));
		return NOS_RESULT_SUCCESS;
	}
	nosResult RegisterFreeLayout(nosNodeFunctions* fn)
	{
		fn->ClassName = NSN_FreeLayout;
		fn->ExecuteNode = ExecuteFreeLayout;
		return NOS_RESULT_SUCCESS;
	}
	nosResult NOSAPI_CALL ExecuteQuadLayout(void* _, nosNodeExecuteParams* params)
	{
		nos::NodeExecuteParams args(params);
		layout::TLayoutDrawList drawList{};
		drawList.items.reserve(4);
		for (size_t i = 0; i < 4; i++)
		{
			float x = 0.0f + (i % 2) * 0.5f;
			float y = 0.0f + (i / 2) * 0.5f;
			layout::LayoutTexturedQuadDrawItem drawItem{};
			drawItem.mutable_position() = nos::fb::vec2(x, y);
			drawItem.mutable_size() = nos::fb::vec2(0.5f, 0.5f);
			drawItem.mutate_texture_id(i);
			drawList.items.push_back(std::move(drawItem));
		}
		nosEngine.SetPinValueByName(args.NodeId, NOS_NAME("OutLayout"), nos::Buffer::From(drawList));
		return NOS_RESULT_SUCCESS;
	}
	nosResult RegisterQuadLayout(nosNodeFunctions* fn)
	{
		fn->ClassName = NSN_QuadLayout;
		fn->ExecuteNode = ExecuteQuadLayout;
		return NOS_RESULT_SUCCESS;
	}
}
