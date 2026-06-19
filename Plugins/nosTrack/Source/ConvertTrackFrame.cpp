// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Helpers.hpp>
#include <nosSysTrack/Track_generated.h>
#include <glm/glm.hpp>

#include <nosGraphics/CoordinateFrameConversion.hpp>

namespace nos::track
{

void RegisterConvertTrackFrame(nosNodeFunctions* funcs)
{
	funcs->ClassName = NOS_NAME("ConvertTrackFrame");
	funcs->ExecuteNode = [](void*, nosNodeExecuteParams* params) {
		auto pins = GetPinValues(params);
		auto ids = GetPinIds(params);

		auto* inTrack = flatbuffers::GetMutableRoot<nos::sys::track::Track>(pins[NOS_NAME("In")]);
		auto source = *static_cast<nos::graphics::Frame*>(pins[NOS_NAME("Source")]);
		auto target = *static_cast<nos::graphics::Frame*>(pins[NOS_NAME("Target")]);

		nos::sys::track::TTrack out;
		inTrack->UnPackTo(&out);

		if (!nos::graphics::CoordinateFrameValid(source) || !nos::graphics::CoordinateFrameValid(target))
		{
			nosEngine.LogE("ConvertTrackFrame: Source/Target frame invalid (forward and up must be on different axes)");
			return nosEngine.SetPinValue(ids[NOS_NAME("Out")], nos::Buffer::From(out));
		}

		const nos::graphics::FrameConvert conv = nos::graphics::MakeFrameConvert(source, target);

		// Location: basis change, then unit conversion derived from the two systems'
		// meters_per_unit. Other Track fields (fov, focus, sensor_size,
		// lens_distortion, ...) are unaffected.
		const auto& inLoc = *inTrack->location();
		glm::dvec3 outLoc = nos::graphics::ConvertPosition(conv, glm::dvec3(inLoc.x(), inLoc.y(), inLoc.z()));
		out.location.mutate_x(static_cast<float>(outLoc.x));
		out.location.mutate_y(static_cast<float>(outLoc.y));
		out.location.mutate_z(static_cast<float>(outLoc.z));

		// Rotation: decode source Euler -> matrix, conjugate by M, re-encode as
		// target Euler.
		const auto& inRot = *inTrack->rotation();
		glm::dmat3 R_src = nos::graphics::EulerToMat(source.euler(), glm::dvec3(inRot.x(), inRot.y(), inRot.z()));
		glm::dvec3 outRotDeg = nos::graphics::MatToEuler(target.euler(), nos::graphics::ConvertRotation(conv, R_src));
		out.rotation.mutate_x(static_cast<float>(outRotDeg.x));
		out.rotation.mutate_y(static_cast<float>(outRotDeg.y));
		out.rotation.mutate_z(static_cast<float>(outRotDeg.z));

		return nosEngine.SetPinValue(ids[NOS_NAME("Out")], nos::Buffer::From(out));
	};
}

}  // namespace nos::track
