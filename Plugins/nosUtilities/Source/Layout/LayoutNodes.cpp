// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include "Layout_generated.h"

NOS_REGISTER_NAME(FreeLayout)
NOS_REGISTER_NAME(QuadLayout)
NOS_REGISTER_NAME(OutDrawItems)
NOS_REGISTER_NAME(GridLayout)
NOS_REGISTER_NAME(FreeOutputLayout)
NOS_REGISTER_NAME(GridOutputLayout)
NOS_REGISTER_NAME(OutputInfos)
namespace nos::utilities
{
	template<typename T>
	nos::Buffer PackPinVectorOfStructs(const std::vector<T>& items)
	{
		flatbuffers::FlatBufferBuilder fbb;
		fbb.Finish(fbb.CreateVectorOfStructs(items));
		auto buf = fbb.Release();
		auto vec = flatbuffers::GetRoot<flatbuffers::Vector<T>>(buf.data());
		return nosBuffer{.Data = (void*)vec, .Size = buf.size() - ((uint8_t*)vec - buf.data())};
	}

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
		nosEngine.SetPinValueByName(args.NodeId, NSN_OutDrawItems, PackPinVectorOfStructs(drawItems));
		return NOS_RESULT_SUCCESS;
	}
	nosResult RegisterFreeLayout(nosNodeFunctions* fn)
	{
		fn->ClassName = NSN_FreeLayout;
		fn->ExecuteNode = ExecuteFreeLayout;
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
		nosEngine.SetPinValueByName(args.NodeId, NSN_OutDrawItems, PackPinVectorOfStructs(drawItems));
		return NOS_RESULT_SUCCESS;
	}
	nosResult RegisterGridLayout(nosNodeFunctions* fn)
	{
		fn->ClassName = NSN_GridLayout;
		fn->ExecuteNode = ExecuteGridLayout;
		return NOS_RESULT_SUCCESS;
	}
	nosResult NOSAPI_CALL ExecuteFreeOutputLayout(void* _, nosNodeExecuteParams* params)
	{
		nos::NodeExecuteParams args(params);
		auto items = args.GetPinData<flatbuffers::Vector<const layout::FreeOutputItem*>>(NOS_NAME("Items"));
		auto totalResolution = args.GetPinData<nos::fb::vec2u>(NOS_NAME("Resolution"));

		std::vector<layout::LayoutOutputInfo> outputs;
		outputs.reserve(items->size());
		for (auto itemPtr : *items)
		{
			auto& item = *itemPtr;
			auto& layoutItem = item.item();
			auto& resolution = item.resolution();

			// Calculate normalized UVs based on total resolution
			float u0 = layoutItem.position().x() / (float)totalResolution->x();
			float v0 = layoutItem.position().y() / (float)totalResolution->y();
			float u1 = (layoutItem.position().x() + layoutItem.size().x()) / (float)totalResolution->x();
			float v1 = (layoutItem.position().y() + layoutItem.size().y()) / (float)totalResolution->y();

			layout::LayoutOutputInfo output{};
			output.mutable_resolution() = resolution;
			output.mutable_pos() = nos::fb::vec2(u0, v0);
			output.mutable_size() = nos::fb::vec2(u1 - u0, v1 - v0);
			outputs.push_back(std::move(output));
		}
		nosEngine.SetPinValueByName(args.NodeId, NSN_OutputInfos, PackPinVectorOfStructs(outputs));
		return NOS_RESULT_SUCCESS;
	}
	nosResult RegisterFreeOutputLayout(nosNodeFunctions* fn)
	{
		fn->ClassName = NSN_FreeOutputLayout;
		fn->ExecuteNode = ExecuteFreeOutputLayout;
		return NOS_RESULT_SUCCESS;
	}
	nosResult NOSAPI_CALL ExecuteGridOutputLayout(void* _, nosNodeExecuteParams* params)
	{
		nos::NodeExecuteParams args(params);
		uint32_t columns = *args.GetPinData<uint32_t>(NOS_NAME("Columns"));
		uint32_t rows = *args.GetPinData<uint32_t>(NOS_NAME("Rows"));
		if (columns == 0 || rows == 0) return NOS_RESULT_SUCCESS;

		auto items = args.GetPinData<flatbuffers::Vector<const layout::GridOutputItem*>>(NOS_NAME("Items"));
		std::vector<layout::LayoutOutputInfo> outputs;
		outputs.reserve(items->size());
		for (auto itemPtr : *items)
		{
			auto& item = *itemPtr;
			auto& layoutItem = item.item();
			auto& resolution = item.resolution();

			// Calculate normalized UVs based on grid size
			float u0 = (float)layoutItem.start().x() / columns;
			float v0 = (float)layoutItem.start().y() / rows;
			float u1 = (float)(layoutItem.start().x() + layoutItem.span().x()) / columns;
			float v1 = (float)(layoutItem.start().y() + layoutItem.span().y()) / rows;

			layout::LayoutOutputInfo output{};
			output.mutable_resolution() = resolution;
			output.mutable_pos() = nos::fb::vec2(u0, v0);
			output.mutable_size() = nos::fb::vec2(u1 - u0, v1 - v0);
			outputs.push_back(std::move(output));
		}
		nosEngine.SetPinValueByName(args.NodeId, NSN_OutputInfos, PackPinVectorOfStructs(outputs));
		return NOS_RESULT_SUCCESS;
	}
	nosResult RegisterGridOutputLayout(nosNodeFunctions* fn)
	{
		fn->ClassName = NSN_GridOutputLayout;
		fn->ExecuteNode = ExecuteGridOutputLayout;
		return NOS_RESULT_SUCCESS;
	}
}