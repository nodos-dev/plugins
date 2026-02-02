// Copyright Zero Density AS. All Rights Reserved.
#include "Common.h"

namespace nos
{
	void RegisterDilateAlphaNode(nosNodeFunctions* nodeFunctions);
	void RegisterDilateByAlphaNode(nosNodeFunctions* nodeFunctions);
	void RegisterBoxBlurNode(nosNodeFunctions* nodeFunctions);
	void RegisterTextureTransitionNode(nosNodeFunctions* nodeFunctions);
	void RegisterTextureMapperNode(nosNodeFunctions* nodeFunctions);
	void RegisterMixerNode(nosNodeFunctions* nodeFunctions);
	void RegisterInterleaveNode(nosNodeFunctions* nodeFunctions);
	void RegisterCube3DLUTNode(nosNodeFunctions* nodeFunctions);
	void RegisterGrid3DNode(nosNodeFunctions* nodeFunctions);
	void RegisterToggleNode(nosNodeFunctions* nodeFunctions);
	void RegisterCanvasMapperNode(nosNodeFunctions* nodeFunctions);
	void RegisterTemporalBlurNode(nosNodeFunctions* nodeFunctions);

	enum NodeType : int {
		Premultiply,
		Unpremultiply,
		DilateAlpha,
		DilateByAlpha,
		ColorMatrix,
		BoxBlur,
		Crop,
		Mask,
		TextureTransition,
		TextureMapper,
		Mixer,
		Interleave,
		Cube3DLUT,
		Grid3D,
		Toggle,
		CanvasMapper,
		TemporalBlur,
		Count
	};

	nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outSize, nosNodeFunctions** outList)
	{
		*outSize = (size_t)NodeType::Count;
		if (!outList)
			return NOS_RESULT_SUCCESS;

		for (int i = 0; i < NodeType::Count; ++i)
		{
			auto node = outList[i];
			switch ((NodeType)i)
			{
			case NodeType::Premultiply:
			{
				node->ClassName = NOS_NAME_STATIC("nos.Utilities2.Premultiply");
				break;
			}
			case NodeType::Unpremultiply:
			{
				node->ClassName = NOS_NAME_STATIC("nos.Utilities2.Unpremultiply");
				break;
			}
			case NodeType::DilateAlpha:
			{
				RegisterDilateAlphaNode(node);
				break;
			}
			case NodeType::DilateByAlpha:
			{
				RegisterDilateByAlphaNode(node);
				break;
			}
			case NodeType::ColorMatrix:
			{
				node->ClassName = NOS_NAME_STATIC("nos.Utilities2.ColorMatrix");
				break;
			}
			case NodeType::BoxBlur:
			{
				RegisterBoxBlurNode(node);
				break;
			}
			case NodeType::Crop:
			{
				node->ClassName = NOS_NAME_STATIC("nos.Utilities2.Crop");
				break;
			}
			case NodeType::Mask:
			{
				node->ClassName = NOS_NAME_STATIC("nos.Utilities2.Mask");
				break;
			}
			case NodeType::TextureTransition:
			{
				RegisterTextureTransitionNode(node);
				break;
			}
			case NodeType::TextureMapper:
			{
				RegisterTextureMapperNode(node);
				break;
			}
			case NodeType::Mixer:
			{
				RegisterMixerNode(node);
				break;
			}
			case NodeType::Interleave:
			{
				RegisterInterleaveNode(node);
				break;
			}
			case NodeType::Cube3DLUT:
			{
				RegisterCube3DLUTNode(node);
				break;
			}
			case NodeType::Grid3D:
			{
				RegisterGrid3DNode(node);
				break;
			}
			case NodeType::Toggle:
			{
				RegisterToggleNode(node);
				break;
			}
			case NodeType::CanvasMapper:
			{
				RegisterCanvasMapperNode(node);
				break;
			}
			case NodeType::TemporalBlur:
			{
				RegisterTemporalBlurNode(node);
				break;
			}
			};
		}
		return NOS_RESULT_SUCCESS;
	}

	extern "C"
	NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* outFunctions)
	{
		outFunctions->ExportNodeFunctions = ExportNodeFunctions;
		return NOS_RESULT_SUCCESS;
	}

} // namespace nos
