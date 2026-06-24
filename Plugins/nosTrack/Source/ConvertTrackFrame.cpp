// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <nosTrack/Track_generated.h>
#include <glm/glm.hpp>

#include <nosTrack/CoordinateFrameConversion.hpp>

namespace nos::track
{

void RegisterConvertTrackFrame(nosNodeFunctions* funcs)
{
	funcs->ClassName = NOS_NAME("ConvertTrackFrame");
	funcs->ExecuteNode = [](void*, nosNodeExecuteParams* params) {
		nos::NodeExecuteParams pins(params);

		nos::track::TTrack out = pins.GetPinValue<nos::track::TTrack>(NOS_NAME("In"));
		auto source = *pins.GetPinValue<nos::track::Frame>(NOS_NAME("Source"));
		auto target = *pins.GetPinValue<nos::track::Frame>(NOS_NAME("Target"));

		if (!nos::track::CoordinateFrameValid(source) || !nos::track::CoordinateFrameValid(target))
		{
			nosEngine.LogE("ConvertTrackFrame: Source/Target frame invalid (forward and up must be on different axes)");
			nosEngine.SetPinValue(pins[NOS_NAME("Out")].Id, nos::Buffer::From(out));
			return NOS_RESULT_SUCCESS;
		}

		const nos::track::FrameConvert conv = nos::track::MakeFrameConvert(source, target);

		// Location: basis change, then unit conversion derived from the two systems'
		// meters_per_unit. Other Track fields (fov, focus, sensor_size,
		// lens_distortion, ...) are unaffected.
		const auto inLoc = out.location;
		glm::dvec3 outLoc = nos::track::ConvertPosition(conv, glm::dvec3(inLoc.x(), inLoc.y(), inLoc.z()));
		out.location.mutate_x(static_cast<float>(outLoc.x));
		out.location.mutate_y(static_cast<float>(outLoc.y));
		out.location.mutate_z(static_cast<float>(outLoc.z));

		// Rotation: decode source Euler -> matrix, conjugate by M, re-encode as
		// target Euler.
		const auto inRot = out.rotation;
		glm::dmat3 R_src = nos::track::EulerToMat(source.euler(), glm::dvec3(inRot.x(), inRot.y(), inRot.z()));
		glm::dvec3 outRotDeg = nos::track::MatToEuler(target.euler(), nos::track::ConvertRotation(conv, R_src));
		out.rotation.mutate_x(static_cast<float>(outRotDeg.x));
		out.rotation.mutate_y(static_cast<float>(outRotDeg.y));
		out.rotation.mutate_z(static_cast<float>(outRotDeg.z));

		nosEngine.SetPinValue(pins[NOS_NAME("Out")].Id, nos::Buffer::From(out));
		return NOS_RESULT_SUCCESS;
	};
}

}  // namespace nos::track
