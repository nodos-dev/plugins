// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <Nodos/Plugin.hpp>

// Framework builtins (nos.fb.vec3d / nos.fb.vec4d)
#include <Builtins_generated.h>
// nos.graphics.TransformQ and the CoordinateFrame struct (generated from
// nos.graphics' Graphics.fbs).
#include <nosTrack/Coordinates_generated.h>
// BasisMatrix() / UnitFactor() for the source -> output system change, plus the
// UNREAL_SYSTEM / GLTF_SYSTEM presets used as detection bases.
#include <nosTrack/CoordinateFrameConversion.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Vendored FBX reader
#include <ofbx.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nos::geometry
{
NOS_REGISTER_NAME(Path)
NOS_REGISTER_NAME(Object)
NOS_REGISTER_NAME(OutputFrame)
NOS_REGISTER_NAME(AutoDetectSource)
NOS_REGISTER_NAME(SourceFrame)
NOS_REGISTER_NAME(LocalTransform)
NOS_REGISTER_NAME(GlobalTransform)
NOS_REGISTER_NAME(DetectedFrame)
NOS_REGISTER_NAME(IsLoaded)

using Frame = nos::track::CoordinateFrame;

// Frame-independent decomposition of an FBX object's transform. Rotation is kept
// as a matrix so it can be re-expressed in whatever CoordinateFrame the user picks
// without re-reading the file.
struct RawTransform
{
	glm::dvec3 Translation{0.0};
	glm::dmat3 Rotation{1.0};
	glm::dvec3 Scale{1.0};
};

struct ObjectTransforms
{
	RawTransform Local;
	RawTransform Global;
};

// Decompose an openFBX 4x4 (column-major double[16]) into translation, a
// normalized rotation matrix, and per-axis scale.
static RawTransform DecomposeMatrix(ofbx::DMatrix const& m)
{
	glm::dmat4 gm(1.0);
	for (int c = 0; c < 4; ++c)
		for (int r = 0; r < 4; ++r)
			gm[c][r] = m.m[c * 4 + r];

	RawTransform out;
	out.Translation = glm::dvec3(gm[3]);
	out.Scale = glm::dvec3(glm::length(glm::dvec3(gm[0])),
						   glm::length(glm::dvec3(gm[1])),
						   glm::length(glm::dvec3(gm[2])));
	out.Rotation = glm::dmat3(
		out.Scale.x != 0.0 ? glm::dvec3(gm[0]) / out.Scale.x : glm::dvec3(1, 0, 0),
		out.Scale.y != 0.0 ? glm::dvec3(gm[1]) / out.Scale.y : glm::dvec3(0, 1, 0),
		out.Scale.z != 0.0 ? glm::dvec3(gm[2]) / out.Scale.z : glm::dvec3(0, 0, 1));
	return out;
}

// Map an .fbx header's axis system + units onto a CoordinateFrame. The engine's
// preset bases are Z-up left-handed (Unreal) and Y-up right-handed (glTF), so the
// up-axis is the deciding signal for the basis; openFBX's own header advises
// ignoring FrontAxis (unreliable across exporters), so we key off UpAxis only. The
// preset's unit is replaced with the file's own: FBX UnitScaleFactor is centimeters
// per unit (default 1.0 => cm), so meters_per_unit = UnitScaleFactor / 100.
static Frame DetectSystem(ofbx::GlobalSettings const* gs)
{
	Frame base = (gs && gs->UpAxis == ofbx::UpVector_AxisZ) ? nos::track::UNREAL_SYSTEM
															: nos::track::GLTF_SYSTEM;
	double mpu = 0.01; // FBX default UnitScaleFactor 1.0 (cm)
	if (gs && gs->UnitScaleFactor > 0.0f)
		mpu = static_cast<double>(gs->UnitScaleFactor) / 100.0;
	return Frame(base.up(), base.forward(), base.handedness(), base.euler(), mpu);
}

// Convert a frame-independent transform into `target`, applying a uniform
// translation scale. M is a signed axis permutation, so:
//   translation -> M * t      rotation -> M * R * M^T      scale -> |M| * s
// The rotation conjugation stays a proper rotation even across a handedness flip
// (det(M)^2 = 1), so quat_cast is well-defined.
static nos::track::TransformQ ConvertToTransformQ(RawTransform const& t, glm::dmat3 const& M, double scale)
{
	glm::dvec3 outT = M * t.Translation * scale;
	glm::dmat3 outR = M * t.Rotation * glm::transpose(M);

	glm::dmat3 absM(0.0);
	for (int c = 0; c < 3; ++c)
		for (int r = 0; r < 3; ++r)
			absM[c][r] = std::abs(M[c][r]);
	glm::dvec3 outS = absM * t.Scale;

	glm::dquat q = glm::normalize(glm::quat_cast(outR));
	return nos::track::TransformQ(
		fb::vec3d(outT.x, outT.y, outT.z),
		fb::vec4d(q.x, q.y, q.z, q.w),
		fb::vec3d(outS.x, outS.y, outS.z));
}

struct ReadFBXTransformNode : NodeContext
{
	// Combo-box labels in display order and their resolved (raw) transforms.
	std::vector<std::string> ObjectLabels;
	std::unordered_map<std::string, ObjectTransforms> Transforms;
	// Per-node unique name of the string list backing the Object combo box.
	std::string ComboListName;
	// Object the user last selected; preferred when (re)loading a file.
	std::string DesiredSelection;

	// System settings. The effective source is Detected when AutoDetect is on,
	// otherwise SourceOverride; the result is expressed in OutputFrame. The unit
	// conversion is derived from the source and output systems' meters_per_unit.
	Frame OutputFrame = nos::track::UNREAL_SYSTEM;
	bool AutoDetect = true;
	Frame SourceOverride = nos::track::GLTF_SYSTEM;
	// Source system read from the loaded file's header (basis + meters_per_unit).
	Frame Detected = nos::track::GLTF_SYSTEM;
	bool Loaded = false;

	// File info is surfaced as a set of short status lines (one slot each) rather than a
	// single string, so the file, object count, frame and selection update independently.
	// The set is pushed to the engine only when it actually changes (see FlushStatus).
	enum class StatusSlot : int { State = 0, File = 1, Objects = 2, Frame = 3, Selected = 4 };
	std::map<StatusSlot, fb::TNodeStatusMessage> StatusMessages;
	bool StatusDirty = false;

	void SetStatus(StatusSlot slot, fb::NodeStatusMessageType type, std::string text)
	{
		auto it = StatusMessages.find(slot);
		if (it != StatusMessages.end() && it->second.type == type && it->second.text == text)
			return; // unchanged - leave the dirty flag alone
		StatusMessages[slot] = fb::TNodeStatusMessage{ {}, std::move(text), type };
		StatusDirty = true;
	}

	void ClearStatus(StatusSlot slot)
	{
		if (StatusMessages.erase(slot))
			StatusDirty = true;
	}

	// Pushes the message set to the engine only when it changed (map order = display order).
	void FlushStatus()
	{
		if (!StatusDirty)
			return;
		std::vector<fb::TNodeStatusMessage> messages;
		messages.reserve(StatusMessages.size());
		for (auto& [slot, msg] : StatusMessages)
			messages.push_back(msg);
		SetNodeStatusMessages(messages);
		StatusDirty = false;
	}

	// System the output is converted from, given the auto-detect toggle.
	Frame EffectiveSource() const { return AutoDetect ? Detected : SourceOverride; }

	// Reflect the auto-detect toggle in the UI: when on, the manual Source Frame pin
	// is ignored (the detected system wins), so grey it out as passive.
	void ApplySourceOrphanState()
	{
		SetPinOrphanState(NSN_SourceFrame,
						  AutoDetect ? fb::PinOrphanStateType::PASSIVE : fb::PinOrphanStateType::ACTIVE,
						  AutoDetect ? "Ignored while Auto-Detect Source is on" : nullptr);
	}

	// Initialization lives in OnCreate (not the constructor) because LoadFbx calls
	// SetPinValue, which the engine broadcasts synchronously back into this node's
	// OnPinValueChanged. With the SAFE binding the engine has already stored our
	// context pointer by the time OnCreate runs, so that re-entrant callback sees a
	// valid `this`. Running it from the constructor would re-enter with a null ctx.
	nosResult OnCreate(nosFbNodePtr node) override
	{
		ComboListName = "nos.geometry.FBXObjects." + std::string(NodeId);
		SetPinVisualizer(NSN_Object, {.type = nos::fb::VisualizerType::COMBO_BOX, .name = ComboListName});

		// Restore saved path / selection / system settings.
		std::string path;
		if (auto* pins = node->pins())
			for (auto const* pin : *pins)
			{
				if (!pin->data() || !pin->data()->size())
					continue;
				auto const* name = pin->name()->c_str();
				auto const* data = pin->data()->data();
				if (0 == strcmp(name, "Path"))
					path = reinterpret_cast<const char*>(data);
				else if (0 == strcmp(name, "Object"))
					DesiredSelection = reinterpret_cast<const char*>(data);
				else if (0 == strcmp(name, "OutputFrame"))
					OutputFrame = *reinterpret_cast<const Frame*>(data);
				else if (0 == strcmp(name, "AutoDetectSource"))
					AutoDetect = *reinterpret_cast<const uint8_t*>(data) != 0;
				else if (0 == strcmp(name, "SourceFrame"))
					SourceOverride = *reinterpret_cast<const Frame*>(data);
			}
		ApplySourceOrphanState();
		if (!path.empty())
			LoadFbx(path);
		return NOS_RESULT_SUCCESS;
	}

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value) override
	{
		if (pinName == NSN_Path)
		{
			LoadFbx(value.Data ? reinterpret_cast<const char*>(value.Data) : "");
			return;
		}
		else if (pinName == NSN_Object)
		{
			DesiredSelection = value.Data ? reinterpret_cast<const char*>(value.Data) : "";
		}
		else if (pinName == NSN_OutputFrame)
		{
			if (value.Data)
				OutputFrame = *reinterpret_cast<const Frame*>(value.Data);
		}
		else if (pinName == NSN_AutoDetectSource)
		{
			if (value.Data)
				AutoDetect = *reinterpret_cast<const uint8_t*>(value.Data) != 0;
			ApplySourceOrphanState();
		}
		else if (pinName == NSN_SourceFrame)
		{
			if (value.Data)
				SourceOverride = *reinterpret_cast<const Frame*>(value.Data);
		}
		else
			return;

		UpdateOutputs(DesiredSelection);
	}

	void LoadFbx(std::string const& path)
	{
		ObjectLabels.clear();
		Transforms.clear();
		UpdateStringList(ComboListName, {});

		std::error_code ec;
		if (path.empty() || !std::filesystem::exists(path, ec))
		{
			Finish(false, path.empty() ? "No FBX file selected" : "FBX file not found");
			return;
		}

		std::ifstream file(std::filesystem::path(path), std::ios::binary);
		std::vector<ofbx::u8> content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		if (content.empty())
		{
			Finish(false, "Failed to read FBX file");
			return;
		}

		// We only need the scene graph / transforms, so skip the heavy payloads.
		auto flags = ofbx::LoadFlags::IGNORE_GEOMETRY | ofbx::LoadFlags::IGNORE_BLEND_SHAPES |
					 ofbx::LoadFlags::IGNORE_TEXTURES | ofbx::LoadFlags::IGNORE_SKIN |
					 ofbx::LoadFlags::IGNORE_MATERIALS | ofbx::LoadFlags::IGNORE_ANIMATIONS |
					 ofbx::LoadFlags::IGNORE_VIDEOS | ofbx::LoadFlags::IGNORE_POSES;

		ofbx::IScene* scene = ofbx::load(content.data(), static_cast<int>(content.size()), static_cast<ofbx::u16>(flags));
		if (!scene)
		{
			nosEngine.LogE("ReadFBXTransform: failed to parse '%s': %s", path.c_str(), ofbx::getError());
			Finish(false, "Failed to parse FBX");
			return;
		}

		Detected = DetectSystem(scene->getGlobalSettings());

		std::unordered_set<std::string> usedLabels;
		int count = scene->getAllObjectCount();
		ofbx::Object const* const* objects = scene->getAllObjects();
		for (int i = 0; i < count; ++i)
		{
			ofbx::Object const* obj = objects[i];
			if (!obj || !obj->isNode())
				continue;

			std::string base = (obj->name[0] != '\0') ? obj->name : "(unnamed)";
			std::string label = base;
			for (int suffix = 2; usedLabels.count(label); ++suffix)
				label = base + " (" + std::to_string(suffix) + ")";
			usedLabels.insert(label);

			ObjectTransforms t;
			t.Local = DecomposeMatrix(obj->getLocalTransform());
			t.Global = DecomposeMatrix(obj->getGlobalTransform());

			Transforms.emplace(label, t);
			ObjectLabels.push_back(std::move(label));
		}
		scene->destroy();

		if (ObjectLabels.empty())
		{
			Finish(false, "No objects found in FBX");
			return;
		}

		UpdateStringList(ComboListName, ObjectLabels);

		// File info lines: name, object count and the detected authoring frame.
		SetStatus(StatusSlot::File, fb::NodeStatusMessageType::INFO,
				  std::filesystem::path(path).filename().string());
		SetStatus(StatusSlot::Objects, fb::NodeStatusMessageType::INFO,
				  std::to_string(ObjectLabels.size()) + " object(s)");
		PublishDetectedFrame();

		// Keep the previous selection if it still exists, otherwise pick the first.
		std::string selection = Transforms.count(DesiredSelection) ? DesiredSelection : ObjectLabels.front();
		DesiredSelection = selection;
		SetPinValue(NSN_Object, selection.c_str());
		UpdateOutputs(selection);

		Finish(true, "Loaded");
	}

	void UpdateOutputs(std::string const& selection)
	{
		auto it = Transforms.find(selection);
		if (it == Transforms.end())
			return;

		const Frame source = EffectiveSource();
		const glm::dmat3 S_src = nos::track::BasisMatrix(source);
		const glm::dmat3 S_tgt = nos::track::BasisMatrix(OutputFrame);
		const glm::dmat3 M = S_tgt * glm::inverse(S_src);
		const double scale = nos::track::UnitFactor(source, OutputFrame);

		SetPinValue(NSN_LocalTransform, nos::Buffer::From(ConvertToTransformQ(it->second.Local, M, scale)));
		SetPinValue(NSN_GlobalTransform, nos::Buffer::From(ConvertToTransformQ(it->second.Global, M, scale)));

		// Selected object plus a compact readout of its converted global translation.
		glm::dvec3 gt = M * it->second.Global.Translation * scale;
		char buf[160];
		std::snprintf(buf, sizeof(buf), "%s  T(%.3g, %.3g, %.3g)", selection.c_str(), gt.x, gt.y, gt.z);
		SetStatus(StatusSlot::Selected, fb::NodeStatusMessageType::INFO, buf);
		PublishDetectedFrame();
		FlushStatus();
	}

	// Surface the detected source system (basis + unit scale), both on the output pin
	// and as a status line. A note marks whether it is actually driving the conversion.
	void PublishDetectedFrame()
	{
		char buf[96];
		std::snprintf(buf, sizeof(buf), "%s up, %s, %g m/unit",
					  nos::track::EnumNameSignedAxis(Detected.up()),
					  nos::track::EnumNameHandedness(Detected.handedness()),
					  Detected.meters_per_unit());
		std::string text = buf;
		if (!AutoDetect)
			text += "  (override active)";
		SetPinValue(NSN_DetectedFrame, text.c_str());
		SetStatus(StatusSlot::Frame, fb::NodeStatusMessageType::INFO, std::string("Frame: ") + text);
	}

	void Finish(bool ok, std::string const& message)
	{
		Loaded = ok;
		SetPinValue(NSN_IsLoaded, nos::Buffer::From(Loaded));
		SetStatus(StatusSlot::State, ok ? fb::NodeStatusMessageType::INFO : fb::NodeStatusMessageType::WARNING,
				  message);
		if (!ok)
		{
			// No valid file: drop the file-info lines, leave only the State line.
			ClearStatus(StatusSlot::File);
			ClearStatus(StatusSlot::Objects);
			ClearStatus(StatusSlot::Frame);
			ClearStatus(StatusSlot::Selected);
		}
		FlushStatus();
	}
};

nosResult RegisterReadFBXTransform(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.geometry.ReadFBXTransform"), ReadFBXTransformNode, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::geometry
