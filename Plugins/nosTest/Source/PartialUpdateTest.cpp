// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginAPI.h>

#include <Builtins_generated.h>

#include <Nodos/PluginHelpers.hpp>


// Nodos SDK
#include <PluginConfig_generated.h>
#include <nosVulkanSubsystem/Types_generated.h>

namespace nos::test
{
NOS_REGISTER_NAME(Type)
NOS_REGISTER_NAME(Input)
NOS_REGISTER_NAME(PartialUpdateTest)
struct PartialUpdateTestNode : NodeContext
{
    std::optional<nos::TypeInfo> Type = {};
    nos::Name VisualizerName = {};

    nosResult OnCreate(const fb::Node* node) override
    {
		return NOS_RESULT_SUCCESS;
    }

	std::vector<nosName> AllTypeNames;


    void TwoElementArray() {
        auto pinId = GetPinId(NSN_Input);
        if (!pinId)
            nosEngine.LogE("Pin not found!");
        nosBuffer buf;
        if (nosEngine.GetDefaultValueOfType(nos::Name("nos.sys.vulkan.BlendMode"), &buf) != NOS_RESULT_SUCCESS) {
            nosEngine.LogE("Failed to get default value of type");
            return;
        }
        nos::sys::vulkan::TBlendMode cppPassInfo = {};
        InterpretPinValue<nos::sys::vulkan::BlendMode>(buf)->UnPackTo(&cppPassInfo);
        cppPassInfo.alpha_op = nos::sys::vulkan::BlendOp::REVERSE_SUBTRACT;
        cppPassInfo.color_op = nos::sys::vulkan::BlendOp::REVERSE_SUBTRACT;
        nos::Buffer finalBuf = nos::Buffer::From(cppPassInfo);
        std::string abc = "bc";
		nosUpdateBufferParams finalBufParams;
		finalBufParams.Action = NOS_BUFFER_UPDATE_ACTION_SET;
        finalBufParams.ActionParams.SetOrInsert.Value = finalBuf;
        nosDataPathComponent pinPath[3] = { { NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("pass3s") }  , { NOS_DATA_PATH_ARRAY_ELEMENT, 1 }, { NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("blend_mode") } };
		finalBufParams.Path = pinPath;
		finalBufParams.PathLength = sizeof(pinPath) / sizeof(pinPath[0]);
		finalBufParams.Target.PinId = *pinId;
		finalBufParams.TargetType = NOS_BUFFER_UPDATE_TARGET_PIN;
        nosEngine.UpdateBuffer(&finalBufParams);
    }

    void SetArraysFixedSizeField() {
        auto pinId = GetPinId(NSN_Input);
        if (!pinId)
            nosEngine.LogE("Pin not found!");

		uint32_t multiSampleCount = 4;
        nos::Buffer finalBuf = nos::Buffer::From(multiSampleCount);
        nosUpdateBufferParams finalBufParams;
        finalBufParams.Action = NOS_BUFFER_UPDATE_ACTION_SET;
        finalBufParams.ActionParams.SetOrInsert.Value = finalBuf;
        nosDataPathComponent pinPath[3] = { { NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("pass3s") } , { NOS_DATA_PATH_ARRAY_ELEMENT, 3 }, { NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("multi_sample_count") } };
        finalBufParams.Path = pinPath;
        finalBufParams.PathLength = sizeof(pinPath) / sizeof(pinPath[0]);
        finalBufParams.Target.PinId = *pinId;
        finalBufParams.TargetType = NOS_BUFFER_UPDATE_TARGET_PIN;
        nosEngine.UpdateBuffer(&finalBufParams);
    }

    void RemoveElement1() {
        auto pinId = GetPinId(NSN_Input);
        if (!pinId)
            nosEngine.LogE("Pin not found!");

        nosUpdateBufferParams finalBufParams;
        finalBufParams.Action = NOS_BUFFER_UPDATE_ACTION_ARRAY_REMOVE;
        nosDataPathComponent pinPath[2] = { { NOS_DATA_PATH_FIELD_COMPONENT, NOS_NAME_STATIC("pass3s") } , {} };
        {
            pinPath[1].Component.ArrayIndex = 1;
            pinPath[1].ComponentType = NOS_DATA_PATH_ARRAY_ELEMENT;
        }
        finalBufParams.Path = pinPath;
        finalBufParams.PathLength = sizeof(pinPath) / sizeof(pinPath[0]);
        finalBufParams.Target.PinId = *pinId;
        finalBufParams.TargetType = NOS_BUFFER_UPDATE_TARGET_PIN;
        nosEngine.UpdateBuffer(&finalBufParams);
    }

    static nosResult GetFunctions(size_t* count, nosName* names, nosPfnNodeFunctionExecute* fns)
    {
        *count = 3;
        if (!names || !fns)
            return NOS_RESULT_SUCCESS;

        names[0] = NOS_NAME_STATIC("2ElementArray");
        fns[0] = [](void* ctx, nosFunctionExecuteParams* params)
            {
                auto writeImage = (PartialUpdateTestNode*)ctx;
                writeImage->TwoElementArray();
                return NOS_RESULT_SUCCESS;
            };

        names[1] = NOS_NAME_STATIC("SetArraysFixedSizeField");
        fns[1] = [](void* ctx, nosFunctionExecuteParams* params)
            {
                auto writeImage = (PartialUpdateTestNode*)ctx;
                writeImage->SetArraysFixedSizeField();
                return NOS_RESULT_SUCCESS;
            };

        names[2] = NOS_NAME_STATIC("RemoveElement1");
        fns[2] = [](void* ctx, nosFunctionExecuteParams* params)
            {
                auto writeImage = (PartialUpdateTestNode*)ctx;
                writeImage->RemoveElement1();
                return NOS_RESULT_SUCCESS;
            };

        return NOS_RESULT_SUCCESS;
    }
};

nosResult RegisterPartialUpdateTest(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_PartialUpdateTest, PartialUpdateTestNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos