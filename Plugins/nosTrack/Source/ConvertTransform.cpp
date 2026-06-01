// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Helpers.hpp>
#include <Builtins_generated.h>
#include <glm/glm.hpp>

#include <cmath>

#include <nosSysTrack/CoordinateFrameConv.h>

namespace nos::track
{

void RegisterConvertTransform(nosNodeFunctions* funcs)
{
	funcs->ClassName = NOS_NAME("ConvertTransform");
	funcs->ExecuteNode = [](void*, nosNodeExecuteParams* params) {
		auto pins = GetPinValues(params);
		auto ids = GetPinIds(params);

		// nos.fb.Transform is a struct, so the pin data is the raw struct bytes.
		auto* in = static_cast<nos::fb::Transform*>(pins[NOS_NAME("In")]);
		auto source = *static_cast<convention::Frame*>(pins[NOS_NAME("SourceFrame")]);
		auto target = *static_cast<convention::Frame*>(pins[NOS_NAME("TargetFrame")]);
		float worldScale = *static_cast<float*>(pins[NOS_NAME("WorldScale")]);

		const glm::dmat3 S_src = convention::BasisMatrix(source);
		const glm::dmat3 S_tgt = convention::BasisMatrix(target);
		const glm::dmat3 M = S_tgt * glm::inverse(S_src);

		// Position: basis change, then uniform world-scale (unit conversion).
		const auto& p = in->position();
		glm::dvec3 outPos = M * glm::dvec3(p.x(), p.y(), p.z()) * static_cast<double>(worldScale);

		// Rotation: build in source frame, conjugate by M (orthogonal => M^-1 = M^T),
		// extract in target frame.
		const auto& r = in->rotation();
		glm::dmat3 R_src = convention::EulerToMat(source, glm::dvec3(r.x(), r.y(), r.z()));
		glm::dmat3 R_tgt = M * R_src * glm::transpose(M);
		glm::dvec3 outRot = convention::MatToEuler(target, R_tgt);

		// Scale: M is a signed axis permutation, so the per-axis factors just reorder.
		const auto& s = in->scale();
		glm::dmat3 absM(0.0);
		for (int c = 0; c < 3; ++c)
			for (int row = 0; row < 3; ++row)
				absM[c][row] = std::abs(M[c][row]);
		glm::dvec3 outScale = absM * glm::dvec3(s.x(), s.y(), s.z());

		nos::fb::Transform out(
			nos::fb::vec3d(outPos.x, outPos.y, outPos.z),
			nos::fb::vec3d(outRot.x, outRot.y, outRot.z),
			nos::fb::vec3d(outScale.x, outScale.y, outScale.z));

		return nosEngine.SetPinValue(ids[NOS_NAME("Out")], nos::Buffer::From(out));
	};
}

}  // namespace nos::track
