// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginAPI.h>

#include <nosAnimationSubsystem/AnimEditorTypes_generated.h>
#include <PinDataAnimator.h>
#include <nosSettingsSubsystem/nosSettingsSubsystem.h>


NOS_INIT()
NOS_SETTINGS_INIT()

NOS_BEGIN_IMPORT_DEPS()
	NOS_SETTINGS_IMPORT()
NOS_END_IMPORT_DEPS()

namespace nos::sys::animation
{
namespace editor
{
	NOS_FBS_CREATE_FUNCTION_MAKE_FOR_UNION(nos::sys::animation::editor, FromAnimation)
}
static std::unique_ptr<struct AnimationSubsystemCtx> GAnimation;

static constexpr char SETTINGS_KEY_FREE_RUN_DELTA_SECS[] = "Free Run Delta Seconds";
nosResult UpdateSettings(const char* entryName, nosBuffer entryValue);

struct AnimationSubsystemCtx
{
	AnimationSubsystemCtx() : InterpolatorManager(), Animator(InterpolatorManager), AnimationSubsystem{}
	{
		AnimationSubsystem.RegisterInterpolator = [](nosAnimationInterpolator const* interpolator) {
			nosPluginInfo callingModule{};
			auto res = nosEngine.GetCallingPlugin(&callingModule);
			if (res != NOS_RESULT_SUCCESS)
			{
				nosEngine.LogE("Failed to get calling module info.");
				return res;
			}
			GAnimation->InterpolatorManager.AddCustomInterpolator(
				{.name = nos::Name(callingModule.Id.Name).AsString(),
				 .version = nos::Name(callingModule.Id.Version).AsString()},
				interpolator->TypeName,
				interpolator->InterpolateCallback);
			return NOS_RESULT_SUCCESS;
		};
		AnimationSubsystem.HasInterpolator = [](nosName typeName, bool* hasHandler) {
			*hasHandler = GAnimation->InterpolatorManager.HasInterpolator(typeName);
			return NOS_RESULT_SUCCESS;
		};
		AnimationSubsystem.Interpolate =
			[](nosName typeName, const nosBuffer from, const nosBuffer to, const double t, nosBuffer* outBuf) {
				std::optional<EngineBuffer> buf;
				auto res = GAnimation->InterpolatorManager.Interpolate(typeName, from, to, t, buf);
				if (res == NOS_RESULT_SUCCESS)
					*outBuf = buf->Release();
				return res;
			};
	}

	void InitSettings()
	{
		settings::RegisterEntry(SETTINGS_KEY_FREE_RUN_DELTA_SECS, "nos.fb.vec2u", UpdateSettings, nos::Buffer::From(FreeRunDeltaSecs), "Free Run Delta Seconds");
	}

	InterpolatorManager InterpolatorManager;
	PinDataAnimator Animator;
	nosAnimationSubsystem AnimationSubsystem;

	nosVec2u FreeRunDeltaSecs = {1, 60};
};


nosResult UpdateSettings(const char* entryName, nosBuffer entryValue)
{
	if (strcmp(SETTINGS_KEY_FREE_RUN_DELTA_SECS, entryName) == 0)
	{
		auto* newVal = static_cast<const nosVec2u*>(entryValue.Data);
		GAnimation->FreeRunDeltaSecs = *newVal;
		return NOS_RESULT_SUCCESS;
	}
	nosEngine.LogW("Settings item not found: %s", entryName);
	return NOS_RESULT_FAILED;
}

nosResult OnRequestAPI(uint32_t minorVersion, void** outPluginAPI)
{
	*outPluginAPI = &GAnimation->AnimationSubsystem;
	return NOS_RESULT_SUCCESS;
}

nosResult OnPreExecuteNode(nosNodeExecuteParams* params)
{
	nosUUID scheduledNodeId;
	if (nosEngine.GetCurrentRunnerPathInfo(&scheduledNodeId, nullptr) == NOS_RESULT_FAILED)
		return NOS_RESULT_FAILED;

	auto pathInfo = GAnimation->Animator.GetPathInfo(scheduledNodeId);

	if(!pathInfo)
		return NOS_RESULT_FAILED;

	uint64_t curFrame = pathInfo->StartFSM + pathInfo->CurFrame;
	nosVec2u deltaSec = GAnimation->FreeRunDeltaSecs;
	if (params->TimingInfo.TimingMode == NOS_EXECUTION_TIMING_MODE_FIXED_STEP)
		deltaSec = params->TimingInfo.FixedStepTiming.DeltaSeconds;
	for (size_t i = 0; i < params->PinCount; ++i)
		if (auto objectBuffer = GetPrimitiveObjectDataView(*params->Pins[i]->ObjectHandle))
			GAnimation->Animator.UpdatePin(params->Pins[i]->Id, deltaSec, curFrame, *objectBuffer);
	return NOS_RESULT_SUCCESS;
}

void NOSAPI_CALL OnPinDeleted(nosUUID pinId)
{
	GAnimation->Animator.OnPinDeleted(pinId);
}

nosResult ShouldExecuteNodeWithoutDirty(nosNodeExecuteParams* params)
{
	for (size_t i = 0; i < params->PinCount; ++i)
	{
		auto const& pinId = params->Pins[i]->Id;
		if(params->Pins[i]->ShowAs == fb::ShowAs::OUTPUT_PIN)
			continue;
		if(GAnimation->Animator.IsPinAnimating(pinId))
			return NOS_RESULT_SUCCESS;
	}
	return NOS_RESULT_NOT_FOUND;
}

void NOSAPI_CALL OnPathStart(nosUUID scheduledPinId)
{
	nosVec2u deltaSec;
	nosEngine.GetCurrentRunnerPathInfo(nullptr, &deltaSec);
	GAnimation->Animator.CreatePathInfo(scheduledPinId, deltaSec);
}

void NOSAPI_CALL OnPathStop(nosUUID scheduledPinId)
{
	GAnimation->Animator.DeletePathInfo(scheduledPinId);
}

void NOSAPI_CALL OnEndFrame(nosUUID scheduledPinId, nosEndFrameCause cause)
{
	if (cause != NOS_END_FRAME_FINISHED)
		return;
	GAnimation->Animator.PathExecutionFinished(scheduledPinId);
}

void OnMessageFromEditor(uint64_t editorId, nosBuffer blob)
{
	auto msg = flatbuffers::GetRoot<editor::FromEditor>(blob.Data);
	switch (msg->event_type())
	{
	case sys::animation::editor::FromEditorUnion::AnimatePin: {
		auto animatePin = msg->event_as_AnimatePin();
		if(!animatePin || !animatePin->pin_path())
			return;
		nosUUID pinId{};
		if (nosEngine.ItemPathToItemId(animatePin->pin_path()->c_str(), &pinId) == NOS_RESULT_SUCCESS)
		{
			nosUUID sourceId{};
			if (nosEngine.GetSourcePinId(pinId, &sourceId) == NOS_RESULT_SUCCESS)
				GAnimation->Animator.AddAnimation(sourceId, *animatePin);
		}
		else
		{
			nosEngine.LogE("Failed to find pin %s", animatePin->pin_path()->c_str());
		}
		break;
	}
	default:
		break;
	}
}

void BroadcastAnimationTypesToEditors()
{
	auto names = GAnimation->InterpolatorManager.GetAnimatableTypes();

	editor::TAnimatableTypes types;
	for (auto& name : names)
		types.types.push_back(name.AsString());
	flatbuffers::FlatBufferBuilder fbb;
	fbb.Finish(editor::MakeFromAnimationOffset(fbb, editor::CreateAnimatableTypes(fbb, &types)));
	nos::Buffer buf = fbb.Release();
	nosSendEditorMessageParams params{.Message = buf, .DispatchType = NOS_EDITOR_MESSAGE_DISPATCH_TYPE_BROADCAST};
	nosEngine.SendEditorMessage(&params);
}

void OnEditorConnected(uint64_t editorId)
{
	BroadcastAnimationTypesToEditors();
}

nosResult NOSAPI_CALL OnPreUnloadPlugin()
{
	GAnimation = nullptr;
	return NOS_RESULT_SUCCESS;
}

void NOSAPI_CALL OnPostOtherPluginUnloaded(nosPluginIdentifier moduleId)
{
	bool typesChanged = GAnimation->InterpolatorManager.PluginUnloaded(
		{ .name = nos::Name(moduleId.Name).AsString(), .version = nos::Name(moduleId.Version).AsString() });
	if (!typesChanged)
		return;
	BroadcastAnimationTypesToEditors();
}
nosResult NOSAPI_CALL OnInitialize()
{
	GAnimation->InitSettings();
	return NOS_RESULT_SUCCESS;
}
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* subsystemFunctions)
{
	subsystemFunctions->OnRequestAPI = nos::sys::animation::OnRequestAPI;
	subsystemFunctions->OnPreExecuteNode = nos::sys::animation::OnPreExecuteNode;
	subsystemFunctions->ShouldExecuteNodeWithoutDirty = nos::sys::animation::ShouldExecuteNodeWithoutDirty;
	subsystemFunctions->OnPathStart = nos::sys::animation::OnPathStart;
	subsystemFunctions->OnPathStop = nos::sys::animation::OnPathStop;
	subsystemFunctions->OnEndFrame = nos::sys::animation::OnEndFrame;
	subsystemFunctions->OnPinDeleted = nos::sys::animation::OnPinDeleted;

	subsystemFunctions->OnMessageFromEditor = nos::sys::animation::OnMessageFromEditor;
	subsystemFunctions->OnEditorConnected = nos::sys::animation::OnEditorConnected;

	subsystemFunctions->OnPreUnloadPlugin = nos::sys::animation::OnPreUnloadPlugin;

	subsystemFunctions->OnPostOtherPluginUnloaded = nos::sys::animation::OnPostOtherPluginUnloaded;
	
	nos::sys::animation::GAnimation = std::make_unique<nos::sys::animation::AnimationSubsystemCtx>();
	subsystemFunctions->Initialize = nos::sys::animation::OnInitialize;
	return NOS_RESULT_SUCCESS;
}
}