// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Helpers.hpp>
#include <Builtins_generated.h>
#include <glm/glm.hpp>

#include <cmath>

#include <nosGraphics/CoordinateFrameConversion.hpp>

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
		auto source = *static_cast<nos::graphics::Frame*>(pins[NOS_NAME("SourceFrame")]);
		auto target = *static_cast<nos::graphics::Frame*>(pins[NOS_NAME("TargetFrame")]);

		if (!nos::graphics::CoordinateFrameValid(source) || !nos::graphics::CoordinateFrameValid(target))
		{
			nosEngine.LogE("ConvertTransform: Source/Target frame invalid (forward and up must be on different axes)");
			return nosEngine.SetPinValue(ids[NOS_NAME("Out")], nos::Buffer::From(*in));
		}

		const nos::graphics::FrameConvert conv = nos::graphics::MakeFrameConvert(source, target);

		// Position: basis change, then unit conversion derived from the two systems.
		const auto& p = in->position();
		glm::dvec3 outPos = nos::graphics::ConvertPosition(conv, glm::dvec3(p.x(), p.y(), p.z()));

		// Rotation: decode source Euler -> matrix, conjugate by M, re-encode as
		// target Euler. The encodings carry the per-frame Euler order/signs.
		const auto& r = in->rotation();
		glm::dmat3 R_src = nos::graphics::EulerToMat(source.euler(), glm::dvec3(r.x(), r.y(), r.z()));
		glm::dvec3 outRot = nos::graphics::MatToEuler(target.euler(), nos::graphics::ConvertRotation(conv, R_src));

		// Scale: M is a signed axis permutation, so the per-axis factors just reorder.
		const auto& s = in->scale();
		glm::dvec3 outScale = nos::graphics::ConvertScale(conv, glm::dvec3(s.x(), s.y(), s.z()));

		nos::fb::Transform out(
			nos::fb::vec3d(outPos.x, outPos.y, outPos.z),
			nos::fb::vec3d(outRot.x, outRot.y, outRot.z),
			nos::fb::vec3d(outScale.x, outScale.y, outScale.z));

		return nosEngine.SetPinValue(ids[NOS_NAME("Out")], nos::Buffer::From(out));
	};
}

}  // namespace nos::track
