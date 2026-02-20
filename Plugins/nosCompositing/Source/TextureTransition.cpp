// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <nosSysVulkan/Helpers.hpp>
#include <glm/ext/scalar_common.hpp>

#include "TextureTransitionUtils.h"
#include "nosCompositing/Transition_generated.h"
#include "Names.h"

namespace nos::compositing
{
// enum class TransitionType
// {
// 	Dissolve,
// 	WipeLeft,
// 	WipeRight,
// 	WipeUp,
// 	WipeDown
// };
//
// enum class TransitionTarget
// {
// 	Input2,
// 	Input1
// };

// Pins and parameters
NOS_REGISTER_NAME(Input1); // nos.sys.vulkan.Texture node input pin + texture shader parameter
NOS_REGISTER_NAME(Input2); // nos.sys.vulkan.Texture node input pin + texture shader parameter

// State
NOS_REGISTER_NAME(IsInTransition);
NOS_REGISTER_NAME(Amount); // float node readonly parameter + float shader parameter
NOS_REGISTER_NAME(TransitionTarget);

// Functions and their arguments
NOS_REGISTER_NAME(DoTransition); // function name
NOS_REGISTER_NAME(Duration); // DoTransition function float parameter
NOS_REGISTER_NAME(Type); // DoTransition function enum parameter
NOS_REGISTER_NAME(WipeWidth); // DoTransition function float parameter
NOS_REGISTER_NAME(Interpolation); // DoTransition function enum parameter
NOS_REGISTER_NAME(EaseExponent); // DoTransition function float parameter
NOS_REGISTER_NAME(StepCount); // DoTransition function uint parameter

// shader and pass
NOS_REGISTER_NAME(TRANSITION_DISSOLVE_PASS);
NOS_REGISTER_NAME(TRANSITION_WIPE_PASS);

// Shader paraemeters
NOS_REGISTER_NAME(InBase); // texture shader parameter
NOS_REGISTER_NAME(UV); // vec2 shader parameter
NOS_REGISTER_NAME(UseX); // int shader parameter
NOS_REGISTER_NAME(Reverse); // int shader parameter
NOS_REGISTER_NAME(HalfWidth); // float shader parameter

struct TextureTransitionContext : public NodeContext
{
	// Transition state
	TransitionInterpolation TransitionInterp = TransitionInterpolation::EaseInOut;
	float TransitionEaseExponent = 2.0f;
	u32 TransitionStepCount = 5;
	TransitionType TransitionType = TransitionType::Dissolve;
	float WipeWidth = 0.4f;
	float TransitionDuration = 1.0f;
	TransitionTarget TransitionTarget = TransitionTarget::Input1;
	bool bIsInTransition = false;
	std::chrono::steady_clock::time_point TransitionStart;


	static nosResult GetFunctions(size_t* outCount, nosName* pName, nosPfnNodeFunctionExecute* outFunction)
	{
		static std::pair<nos::Name, nosPfnNodeFunctionExecute> Functions[1] = {
			{NOS_NAME_STATIC("DoTransition"), TextureTransitionContext::DoTransition}
		};

		*outCount = sizeof(Functions) / sizeof(Functions[0]);

		if (!pName || !outFunction)
			return NOS_RESULT_SUCCESS;

		for (auto& [name, fn] : Functions)
		{
			*pName++ = name;
			*outFunction++ = fn;
		}

		return NOS_RESULT_SUCCESS;
	}


	void UpdateTransition(float elapsed)
	{
		float amount = glm::clamp(elapsed / TransitionDuration, 0.0f, 1.0f);
		amount = GetInterpolation(TransitionInterp, amount, TransitionStepCount, TransitionEaseExponent);
		amount = glm::clamp(amount, 0.0f, 1.0f);
		if (amount >= 1.0f)
		{
			amount = 0.0f;
			TransitionTarget = TransitionTarget == TransitionTarget::Input1
				                   ? TransitionTarget::Input2
				                   : TransitionTarget::Input1;
			bIsInTransition = false;
			nosEngine.SetPinValueByName(NodeId, NSN_IsInTransition, {&bIsInTransition, sizeof(bIsInTransition)});
			nosEngine.SetPinValueByName(NodeId, NSN_TransitionTarget, {&TransitionTarget, sizeof(TransitionTarget)});
		}
		nosEngine.SetPinValueByName(NodeId, NSN_Amount, {&amount, sizeof(amount)});
	}


	nosResult ExecuteNode(nos::NodeExecuteParams const& params)
	{
		nosCmd cmd;
		nosCmdBeginParams bp = {.Name = nos::Name("Transition"), .AssociatedNodeId = NodeId, .OutCmdHandle = &cmd};
		nosVulkan->Begin(&bp);

		if (bIsInTransition)
		{
			auto now = std::chrono::high_resolution_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - TransitionStart);
			UpdateTransition(elapsed.count() / 1000000.0f);
		}

		auto transitionTarget = *params.GetPinData<compositing::TransitionTarget>(NSN_TransitionTarget);

		float amount = *params.GetPinData<float>(NSN_Amount);;
		auto source1Texture = params.GetPinObject(transitionTarget == TransitionTarget::Input2
			                                          ? NSN_Input1.GetName()
			                                          : NSN_Input2.GetName());
		auto source2Texture = params.GetPinObject(transitionTarget == TransitionTarget::Input2
			                                          ? NSN_Input2.GetName()
			                                          : NSN_Input1.GetName());
		auto outputTexture = params.GetPinObject(NSN_Output);

		if (TransitionType == TransitionType::Dissolve)
		{
			nosRunPassParams transitionPassParams = {};
			transitionPassParams.Key = NSN_TRANSITION_DISSOLVE_PASS;
			std::vector bindings = {
				nos::sys::vulkan::ShaderTextureBinding(NSN_Input1, source1Texture, NOS_TEXTURE_FILTER_LINEAR),
				nos::sys::vulkan::ShaderTextureBinding(NSN_Input2, source2Texture, NOS_TEXTURE_FILTER_LINEAR),
				nos::sys::vulkan::ShaderDataBinding(NSN_Amount, amount),
			};
			transitionPassParams.Bindings = bindings.data();
			transitionPassParams.BindingCount = (u32)bindings.size();
			transitionPassParams.Output = outputTexture;
			nosVulkan->RunPass(cmd, &transitionPassParams);
		}
		else
		{
			auto wipeLeft = TransitionType == TransitionType::WipeLeft;
			auto wipeRight = TransitionType == TransitionType::WipeRight;
			auto wipeUp = TransitionType == TransitionType::WipeUp;
			auto wipeDown = TransitionType == TransitionType::WipeDown;

			auto useX = (wipeLeft || wipeRight) ? 1 : 0;
			auto reverse = (wipeLeft || wipeUp) ? 1 : 0;
			auto wipeHalfWidth = 0.5f * WipeWidth;

			if (wipeLeft || wipeRight || wipeUp || wipeDown)
			{
				nosRunPassParams transitionPassParams = {};
				transitionPassParams.Key = NSN_TRANSITION_WIPE_PASS;
				std::vector bindings = {
					nos::sys::vulkan::ShaderTextureBinding(NSN_Input1, source1Texture, NOS_TEXTURE_FILTER_LINEAR),
					nos::sys::vulkan::ShaderTextureBinding(NSN_Input2, source2Texture, NOS_TEXTURE_FILTER_LINEAR),
					nos::sys::vulkan::ShaderDataBinding(NSN_Amount, amount),
					nos::sys::vulkan::ShaderDataBinding(NSN_UseX, useX),
					nos::sys::vulkan::ShaderDataBinding(NSN_Reverse, reverse),
					nos::sys::vulkan::ShaderDataBinding(NSN_HalfWidth, wipeHalfWidth),
				};
				transitionPassParams.Bindings = bindings.data();
				transitionPassParams.BindingCount = (u32)bindings.size();
				transitionPassParams.Output = outputTexture;
				nosVulkan->RunPass(cmd, &transitionPassParams);
			}
		}

		nosVulkan->End(cmd, 0);

		return NOS_RESULT_SUCCESS;
	}


	nosResult CopyFrom(nosCopyFromInfo* copyInfo)
	{
		if (bIsInTransition)
			nosEngine.SetPinDirty(*GetPinId(NSN_Amount));
		return NOS_RESULT_SUCCESS;
	}


	static nosResult DoTransition(void* ctx, nosFunctionExecuteParams* fnParams)
	{
		auto context = (TextureTransitionContext*)ctx;

		nos::NodeExecuteParams params = fnParams->FunctionNodeExecuteParams;
		nos::NodeExecuteParams nodeParams = fnParams->ParentNodeExecuteParams;

		if (context->bIsInTransition)
			return NOS_RESULT_SUCCESS;

		context->TransitionDuration = *params.GetPinData<float>(NSN_Duration);
		context->TransitionType = *params.GetPinData<compositing::TransitionType>(NSN_Type);
		context->WipeWidth = *params.GetPinData<float>(NSN_WipeWidth);
		context->TransitionInterp = *params.GetPinData<TransitionInterpolation>(NSN_Interpolation);
		context->TransitionEaseExponent = *params.GetPinData<float>(NSN_EaseExponent);
		context->TransitionStepCount = *params.GetPinData<u32>(NSN_StepCount);
		context->TransitionTarget = *nodeParams.GetPinData<compositing::TransitionTarget>(NSN_TransitionTarget);
		context->TransitionStart = std::chrono::high_resolution_clock::now();
		context->bIsInTransition = true;
		nosEngine.SetPinValueByName(context->NodeId,
		                            NSN_IsInTransition,
		                            {&context->bIsInTransition, sizeof(context->bIsInTransition)});

		return NOS_RESULT_SUCCESS;
	}
};


void RegisterTextureTransition(nosNodeFunctions* nodeFunctions)
{
	NOS_BIND_NODE_CLASS(NOS_NAME_STATIC("TextureTransition"), TextureTransitionContext, nodeFunctions);
}

} // namespace nos::compositing
