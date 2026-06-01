// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Helpers.hpp>
#include <nosSysTrack/Track_generated.h>
#include <glm/glm.hpp>

#include <nosSysTrack/CoordinateFrameConv.h>

namespace nos::track
{

void RegisterTrackTransform(nosNodeFunctions* funcs)
{
	funcs->ClassName = NOS_NAME("TrackTransform");
	funcs->ExecuteNode = [](void*, nosNodeExecuteParams* params) {
		auto pins = GetPinValues(params);
		auto ids = GetPinIds(params);

		auto* inTrack = flatbuffers::GetMutableRoot<nos::sys::track::Track>(pins[NOS_NAME("In")]);
		auto source = *static_cast<convention::Frame*>(pins[NOS_NAME("Source")]);
		auto target = *static_cast<convention::Frame*>(pins[NOS_NAME("Target")]);
		float worldScale = *static_cast<float*>(pins[NOS_NAME("WorldScale")]);

		nos::sys::track::TTrack out;
		inTrack->UnPackTo(&out);

		const glm::dmat3 S_src = convention::BasisMatrix(source);
		const glm::dmat3 S_tgt = convention::BasisMatrix(target);
		const glm::dmat3 M = S_tgt * glm::inverse(S_src);

		// Location: basis change, then uniform world-scale. Other Track fields
		// (rotation, fov, focus, sensor_size, lens_distortion, ...) are unaffected.
		const auto& inLoc = *inTrack->location();
		glm::dvec3 loc(inLoc.x(), inLoc.y(), inLoc.z());
		glm::dvec3 outLoc = M * loc * static_cast<double>(worldScale);
		out.location.mutate_x(static_cast<float>(outLoc.x));
		out.location.mutate_y(static_cast<float>(outLoc.y));
		out.location.mutate_z(static_cast<float>(outLoc.z));

		// Rotation: build in source frame, conjugate by M, extract in target frame.
		const auto& inRot = *inTrack->rotation();
		glm::dmat3 R_src = convention::EulerToMat(source, glm::dvec3(inRot.x(), inRot.y(), inRot.z()));
		glm::dmat3 R_tgt = M * R_src * glm::transpose(M);
		glm::dvec3 outRotDeg = convention::MatToEuler(target, R_tgt);
		out.rotation.mutate_x(static_cast<float>(outRotDeg.x));
		out.rotation.mutate_y(static_cast<float>(outRotDeg.y));
		out.rotation.mutate_z(static_cast<float>(outRotDeg.z));

		return nosEngine.SetPinValue(ids[NOS_NAME("Out")], nos::Buffer::From(out));
	};
}

}  // namespace nos::track
