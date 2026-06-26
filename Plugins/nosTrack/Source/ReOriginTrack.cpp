// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include "nosTrack/Track_generated.h"

#include <glm/glm.hpp>

#include <nosMath/CoordinateFrameConversion.hpp>

namespace nos::track
{

NOS_REGISTER_NAME(Input);
NOS_REGISTER_NAME(Output);
NOS_REGISTER_NAME(CoordinateSystem);
NOS_REGISTER_NAME(Origin);
NOS_REGISTER_NAME(ReOriginTrack_MarkOrigin);
NOS_REGISTER_NAME(ReOriginTrack_ClearOrigin);

struct ReOriginTrackContext : NodeContext
{
	nos::math::Frame Frame = nos::math::UNREAL_SYSTEM;
	nos::track::TTrack Origin;     // Marked reference pose (identity until marked).
	nos::track::TTrack LatestInput; // Last input seen, captured by Mark Origin.

	nosResult OnCreate(nosFbNodePtr node) override
	{
		if (node->pins())
		{
			for (auto* pin : *node->pins())
			{
				if (!flatbuffers::IsFieldPresent(pin, fb::Pin::VT_DATA))
					continue;
				nosBuffer value = {.Data = (void*)pin->data()->data(), .Size = pin->data()->size()};
				OnPinValueChanged(nos::Name(pin->name()->c_str()), *pin->id(), value);
			}
		}
		UpdateStatus();
		return NOS_RESULT_SUCCESS;
	}

	void OnPinValueChanged(nos::Name pinName, uuid const& pinId, nosBuffer val) override
	{
		if (pinName == NSN_CoordinateSystem)
			Frame = *static_cast<nos::math::Frame*>(val.Data);
		else if (pinName == NSN_Origin)
			flatbuffers::GetRoot<nos::track::Track>(val.Data)->UnPackTo(&Origin);
	}

	bool HasOrigin() const
	{
		auto const& l = Origin.location;
		auto const& r = Origin.rotation;
		return l.x() != 0 || l.y() != 0 || l.z() != 0 || r.x() != 0 || r.y() != 0 || r.z() != 0;
	}

	nosResult ExecuteNode(nos::NodeExecuteParams const& params) override
	{
		auto inBuf = params.GetPinBuffer(NSN_Input);
		if (!inBuf.Data)
			return NOS_RESULT_SUCCESS;

		auto* in = flatbuffers::GetRoot<nos::track::Track>(inBuf.Data);
		if (!in)
			return NOS_RESULT_SUCCESS;
		in->UnPackTo(&LatestInput);

		// Pass non-pose fields through; only location/rotation are re-origined.
		nos::track::TTrack out = LatestInput;

		// Rigid-body relative pose: out = inverse(T_origin) * T_input.
		// transpose(R_origin) inverts the rotation; the translation is the input
		// offset rotated into the origin's local frame. Frame-agnostic given a
		// proper rotation matrix, and no unit conversion (origin and input share
		// one coordinate system).
		auto const& il = LatestInput.location;
		auto const& ol = Origin.location;
		glm::dmat3 R_origin = nos::math::EulerToMat(
			Frame.euler(), glm::dvec3(Origin.rotation.x(), Origin.rotation.y(), Origin.rotation.z()));
		glm::dmat3 R_in = nos::math::EulerToMat(
			Frame.euler(), glm::dvec3(LatestInput.rotation.x(), LatestInput.rotation.y(), LatestInput.rotation.z()));
		glm::dmat3 R_originT = glm::transpose(R_origin);

		glm::dvec3 relPos = R_originT * glm::dvec3(il.x() - ol.x(), il.y() - ol.y(), il.z() - ol.z());
		glm::dvec3 relRot = nos::math::MatToEuler(Frame.euler(), R_originT * R_in);

		out.location = nos::fb::vec3((float)relPos.x, (float)relPos.y, (float)relPos.z);
		out.rotation = nos::fb::vec3((float)relRot.x, (float)relRot.y, (float)relRot.z);

		auto buf = nos::Buffer::From(out);
		SetPinValue(NSN_Output, {.Data = buf.Data(), .Size = buf.Size()});
		return NOS_RESULT_SUCCESS;
	}

	void StoreOrigin()
	{
		auto buf = nos::Buffer::From(Origin);
		SetPinValue(NSN_Origin, {.Data = buf.Data(), .Size = buf.Size()});
	}

	void UpdateStatus()
	{
		if (HasOrigin())
			SetNodeStatusMessage("Origin marked", fb::NodeStatusMessageType::INFO);
		else
			SetNodeStatusMessage("No origin (pass-through)", fb::NodeStatusMessageType::INFO);
	}

	static nosResult GetFunctions(size_t* count, nosName* names, nosPfnNodeFunctionExecute* fns)
	{
		*count = 2;
		if (!names || !fns)
			return NOS_RESULT_SUCCESS;

		names[0] = NOS_NAME_STATIC("ReOriginTrack_MarkOrigin");
		fns[0] = [](void* ctx, nosFunctionExecuteParams*) {
			auto* self = static_cast<ReOriginTrackContext*>(ctx);
			self->Origin = self->LatestInput;
			self->StoreOrigin();
			self->UpdateStatus();
			nosEngine.LogI("ReOriginTrack: Origin marked");
			return NOS_RESULT_SUCCESS;
		};

		names[1] = NOS_NAME_STATIC("ReOriginTrack_ClearOrigin");
		fns[1] = [](void* ctx, nosFunctionExecuteParams*) {
			auto* self = static_cast<ReOriginTrackContext*>(ctx);
			self->Origin = nos::track::TTrack{};
			self->StoreOrigin();
			self->UpdateStatus();
			nosEngine.LogI("ReOriginTrack: Origin cleared");
			return NOS_RESULT_SUCCESS;
		};

		return NOS_RESULT_SUCCESS;
	}
};

void RegisterReOriginTrack(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.track.ReOriginTrack"), ReOriginTrackContext, fn);
}

} // namespace nos::track
