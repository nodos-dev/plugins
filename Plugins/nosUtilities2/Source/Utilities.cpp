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
        BoxBlur,
		DilateAlpha,
		DilateByAlpha,
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

			case NodeType::BoxBlur:
			{
				RegisterBoxBlurNode(node);
				break;
			}

			case NodeType::Mask:
			{
				node->ClassName = NOS_NAME_STATIC("nos.utilities2.Mask");
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


	void GetRenamedTypes(nosName* outRenamedFrom, nosName* outRenamedTo, size_t* outSize)
	{
		static std::vector<std::pair<nos::Name, nos::Name>> renames = {
			{NOS_NAME("zd.utilities.BlendMode"), NOS_NAME("nos.utilities2.BlendMode")},
			{NOS_NAME("zd.utilities.CanvasLayer"), NOS_NAME("nos.utilities2.CanvasLayer")},
			{NOS_NAME("zd.utilities.LUTType"), NOS_NAME("nos.utilities2.LUTType")},
			{NOS_NAME("zd.utilities.MixerChannelType"), NOS_NAME("nos.utilities2.MixerChannelType")},
			{NOS_NAME("zd.utilities.MixerTransitionType"), NOS_NAME("nos.utilities2.MixerTransitionType")},
			{NOS_NAME("zd.utilities.MixerTransitionTarget"), NOS_NAME("nos.utilities2.MixerTransitionTarget")},
			{NOS_NAME("zd.utilities.TransitionType"), NOS_NAME("nos.utilities2.TransitionType")},
			{NOS_NAME("zd.utilities.TransitionInterpolation"), NOS_NAME("nos.utilities2.TransitionInterpolation")},
			{NOS_NAME("zd.utilities.TransitionTarget"), NOS_NAME("nos.utilities2.TransitionTarget")},
		};

		if (!outRenamedFrom)
		{
			*outSize = renames.size();
			return;
		}

		for (size_t i = 0; i < renames.size(); ++i)
		{
			outRenamedFrom[i] = renames[i].first;
			outRenamedTo[i] = renames[i].second;
		}
	}

	void GetRenamedNodeClasses(nosName* outRenamedFrom, nosName* outRenamedTo, size_t* outSize)
	{
		static std::vector<std::pair<nos::Name, nos::Name>> renames = {
			{NOS_NAME("zd.utilities.BoxBlur"), NOS_NAME("nos.utilities2.BoxBlur")},
			{NOS_NAME("zd.utilities.DilateAlpha"), NOS_NAME("nos.utilities2.DilateAlpha")},
			{NOS_NAME("zd.utilities.DilateByAlpha"), NOS_NAME("nos.utilities2.DilateByAlpha")},
			{NOS_NAME("zd.utilities.Mask"), NOS_NAME("nos.utilities2.Mask")},
			{NOS_NAME("zd.utilities.TextureTransition"), NOS_NAME("nos.utilities2.TextureTransition")},
			{NOS_NAME("zd.utilities.TextureMapper"), NOS_NAME("nos.utilities2.TextureMapper")},
			{NOS_NAME("zd.utilities.Mixer"), NOS_NAME("nos.utilities2.Mixer")},
			{NOS_NAME("zd.utilities.Interleave"), NOS_NAME("nos.utilities2.Interleave")},
			{NOS_NAME("zd.utilities.Cube3DLUT"), NOS_NAME("nos.utilities2.Cube3DLUT")},
			{NOS_NAME("zd.utilities.Grid3D"), NOS_NAME("nos.utilities2.Grid3D")},
			{NOS_NAME("zd.utilities.Toggle"), NOS_NAME("nos.utilities2.Toggle")},
			{NOS_NAME("zd.utilities.CanvasMapper"), NOS_NAME("nos.utilities2.CanvasMapper")},
			{NOS_NAME("zd.utilities.TemporalBlur"), NOS_NAME("nos.utilities2.TemporalBlur")},
		};

		if (!outRenamedFrom)
		{
			*outSize = renames.size();
			return;
		}

		for (size_t i = 0; i < renames.size(); ++i)
		{
			outRenamedFrom[i] = renames[i].first;
			outRenamedTo[i] = renames[i].second;
		}
	}

	extern "C"
	NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* outFunctions)
	{
		outFunctions->ExportNodeFunctions = ExportNodeFunctions;

		outFunctions->GetRenamedTypes = GetRenamedTypes;
		outFunctions->GetRenamedNodeClasses = GetRenamedNodeClasses;
		return NOS_RESULT_SUCCESS;
	}

} // namespace nos
