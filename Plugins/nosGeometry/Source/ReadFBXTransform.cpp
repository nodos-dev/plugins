// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <Nodos/Helpers.hpp>

// Framework builtins (nos.fb.vec3d / nos.fb.vec4d)
#include <Builtins_generated.h>
// nos.graphics.TransformQ and the CoordinateFrame enum (generated from
// nos.graphics' Graphics.fbs); the latter documents the FBX authoring frame.
#include <Graphics_generated.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Vendored FBX reader
#include <ofbx.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nos::geometry
{
NOS_REGISTER_NAME(Path)
NOS_REGISTER_NAME(Object)
NOS_REGISTER_NAME(SourceFrame)
NOS_REGISTER_NAME(LocalTransform)
NOS_REGISTER_NAME(GlobalTransform)
NOS_REGISTER_NAME(IsLoaded)

using Frame = nos::graphics::CoordinateFrame;

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

// Express a decomposed transform as a nos.graphics.TransformQ. The rotation is
// emitted as a quaternion (x, y, z, w) taken directly from the rotation matrix,
// which is frame-independent (no per-frame Euler convention or gimbal lock).
static graphics::TransformQ ToTransformQ(RawTransform const& t)
{
	glm::dquat q = glm::normalize(glm::quat_cast(t.Rotation));
	return graphics::TransformQ(
		fb::vec3d(t.Translation.x, t.Translation.y, t.Translation.z),
		fb::vec4d(q.x, q.y, q.z, q.w),
		fb::vec3d(t.Scale.x, t.Scale.y, t.Scale.z));
}

struct ReadFBXTransformContext : NodeContext
{
	// Combo-box labels in display order and their resolved (raw) transforms.
	std::vector<std::string> ObjectLabels;
	std::unordered_map<std::string, ObjectTransforms> Transforms;
	// Per-node unique name of the string list backing the Object combo box.
	std::string ComboListName;
	// Object the user last selected; preferred when (re)loading a file.
	std::string DesiredSelection;
	// Coordinate frame the user declared the file is authored in.
	Frame CurrentFrame = Frame::RH_YUp_FwdNegZ_RightX;
	bool Loaded = false;

	ReadFBXTransformContext(nosFbNodePtr node) : NodeContext(node)
	{
		ComboListName = "nos.geometry.FBXObjects." + std::string(NodeId);
		SetPinVisualizer(NSN_Object, {.type = nos::fb::VisualizerType::COMBO_BOX, .name = ComboListName});

		// Restore saved path / selection / frame.
		std::string path;
		if (auto* pins = node->pins())
			for (auto const* pin : *pins)
			{
				if (!pin->data() || !pin->data()->size())
					continue;
				auto const* name = pin->name()->c_str();
				if (0 == strcmp(name, "Path"))
					path = reinterpret_cast<const char*>(pin->data()->data());
				else if (0 == strcmp(name, "Object"))
					DesiredSelection = reinterpret_cast<const char*>(pin->data()->data());
				else if (0 == strcmp(name, "SourceFrame"))
					CurrentFrame = static_cast<Frame>(*reinterpret_cast<const uint8_t*>(pin->data()->data()));
			}
		if (!path.empty())
			LoadFbx(path);
	}

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer value) override
	{
		if (pinName == NSN_Path)
		{
			LoadFbx(value.Data ? reinterpret_cast<const char*>(value.Data) : "");
		}
		else if (pinName == NSN_Object)
		{
			DesiredSelection = value.Data ? reinterpret_cast<const char*>(value.Data) : "";
			UpdateOutputs(DesiredSelection);
		}
		else if (pinName == NSN_SourceFrame)
		{
			if (value.Data)
				CurrentFrame = static_cast<Frame>(*reinterpret_cast<const uint8_t*>(value.Data));
			UpdateOutputs(DesiredSelection);
		}
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

		// Keep the previous selection if it still exists, otherwise pick the first.
		std::string selection = Transforms.count(DesiredSelection) ? DesiredSelection : ObjectLabels.front();
		DesiredSelection = selection;
		SetPinValue(NSN_Object, selection.c_str());
		UpdateOutputs(selection);

		Finish(true, "Loaded " + std::to_string(ObjectLabels.size()) + " object(s)");
	}

	void UpdateOutputs(std::string const& selection)
	{
		auto it = Transforms.find(selection);
		if (it == Transforms.end())
			return;
		SetPinValue(NSN_LocalTransform, nos::Buffer::From(ToTransformQ(it->second.Local)));
		SetPinValue(NSN_GlobalTransform, nos::Buffer::From(ToTransformQ(it->second.Global)));
	}

	void Finish(bool ok, std::string const& message)
	{
		Loaded = ok;
		SetPinValue(NSN_IsLoaded, nos::Buffer::From(Loaded));
		SetNodeStatusMessage(message, ok ? fb::NodeStatusMessageType::INFO : fb::NodeStatusMessageType::WARNING);
	}
};

nosResult RegisterReadFBXTransform(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.geometry.ReadFBXTransform"), ReadFBXTransformContext, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::geometry
