// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include "Layout_generated.h"

NOS_REGISTER_NAME(FreeLayout)
NOS_REGISTER_NAME(QuadLayout)
NOS_REGISTER_NAME(OutDrawItems)
NOS_REGISTER_NAME(GridLayout)
namespace nos::utilities
{
	nosResult NOSAPI_CALL ExecuteFreeLayout(void* _, nosNodeExecuteParams* params)
	{
		nos::NodeExecuteParams args(params);
		auto& outSize = *args.GetPinData<nos::fb::vec2u>(NOS_NAME("OutputSize"));
		auto items = args.GetPinData<flatbuffers::Vector<const layout::FreeLayoutItem*>>(NOS_NAME("Items"));

		int textureId = 0;
		std::vector<layout::LayoutDrawItem> drawItems;
		drawItems.reserve(items->size());
		for (auto itemPtr : *items)
		{
			auto& item = *itemPtr;
			layout::LayoutDrawItem drawItem{};
			drawItem.mutable_position() = nos::fb::vec2(item.position().x() / (float) outSize.x(), item.position().y() / (float) outSize.y());
			drawItem.mutable_size() = nos::fb::vec2(item.size().x() / (float)outSize.x(), item.size().y() / (float)outSize.y());
			drawItem.mutate_texture_id(textureId);
			drawItems.push_back(std::move(drawItem));
			textureId++;
		}
		flatbuffers::FlatBufferBuilder fbb;
		fbb.Finish(fbb.CreateVectorOfStructs(drawItems));
		auto buf = fbb.Release();
		auto drawInfo = flatbuffers::GetRoot<layout::LayoutDrawInfo>(buf.data());

		nosEngine.SetPinValueByName(args.NodeId, NSN_OutDrawItems, nosBuffer{.Data = (void*)drawInfo, .Size = buf.size() - ((uint8_t*)drawInfo - buf.data())});
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
		std::vector<layout::LayoutDrawItem> drawItems;
		drawItems.reserve(4);
		for (size_t i = 0; i < 4; i++)
		{
			float x = 0.0f + (i % 2) * 0.5f;
			float y = 0.0f + (i / 2) * 0.5f;
			layout::LayoutDrawItem drawItem{};
			drawItem.mutable_position() = nos::fb::vec2(x, y);
			drawItem.mutable_size() = nos::fb::vec2(0.5f, 0.5f);
			drawItem.mutate_texture_id(i);
			drawItems.push_back(std::move(drawItem));
		}
		
		flatbuffers::FlatBufferBuilder fbb;
		fbb.Finish(fbb.CreateVectorOfStructs(drawItems));
		auto buf = fbb.Release();
		auto drawInfo = flatbuffers::GetRoot<layout::LayoutDrawInfo>(buf.data());

		nosEngine.SetPinValueByName(
			args.NodeId,
			NSN_OutDrawItems,
			nosBuffer{.Data = (void*)drawInfo, .Size = buf.size() - ((uint8_t*)drawInfo - buf.data())});
		return NOS_RESULT_SUCCESS;
	}
	nosResult RegisterQuadLayout(nosNodeFunctions* fn)
	{
		fn->ClassName = NSN_QuadLayout;
		fn->ExecuteNode = ExecuteQuadLayout;
		return NOS_RESULT_SUCCESS;
	}
	nosResult NOSAPI_CALL ExecuteGridLayout(void* _, nosNodeExecuteParams* params)
	{
		nos::NodeExecuteParams args(params);
		// Get grid size from input pins
		uint32_t columns = *args.GetPinData<uint32_t>(NOS_NAME("Columns"));
		uint32_t rows = *args.GetPinData<uint32_t>(NOS_NAME("Rows"));
		if (columns == 0 || rows == 0) return NOS_RESULT_SUCCESS;

		// Get items array
		auto items = args.GetPinData<flatbuffers::Vector<const layout::GridLayoutItem*>>(NOS_NAME("Items"));
		std::vector<layout::LayoutDrawItem> drawItems;
		drawItems.reserve(items->size());
		int textureId = 0;
		for (auto itemPtr : *items)
		{
			auto& item = *itemPtr;
			float x = (float)item.start().x() / columns;
			float y = (float)item.start().y() / rows;
			float w = (float)item.span().x() / columns;
			float h = (float)item.span().y() / rows;
			layout::LayoutDrawItem drawItem{};
			drawItem.mutable_position() = nos::fb::vec2(x, y);
			drawItem.mutable_size() = nos::fb::vec2(w, h);
			drawItem.mutate_texture_id(textureId);
			drawItems.push_back(std::move(drawItem));
			textureId++;
		}
		flatbuffers::FlatBufferBuilder fbb;
		fbb.Finish(fbb.CreateVectorOfStructs(drawItems));
		auto buf = fbb.Release();
		auto vec = flatbuffers::GetRoot<flatbuffers::Vector<layout::LayoutDrawItem>>(buf.data());
		nosEngine.SetPinValueByName(args.NodeId, NSN_OutDrawItems, nosBuffer{.Data = (void*)vec, .Size = buf.size() - ((uint8_t*)vec - buf.data())});
		return NOS_RESULT_SUCCESS;
	}
	nosResult RegisterGridLayout(nosNodeFunctions* fn)
	{
		fn->ClassName = NSN_GridLayout;
		fn->ExecuteNode = ExecuteGridLayout;
		return NOS_RESULT_SUCCESS;
	}
}
