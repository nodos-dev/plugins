// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <Builtins_generated.h>
#include <glm/glm.hpp>

#include <cmath>

#include <nosTrack/CoordinateFrameConversion.hpp>

namespace nos::track
{

void RegisterConvertTransform(nosNodeFunctions* funcs)
{
	funcs->ClassName = NOS_NAME("ConvertTransform");
	funcs->ExecuteNode = [](void*, nosNodeExecuteParams* params) {
		nos::NodeExecuteParams pins(params);

		// nos.fb.Transform is a struct, so the pin data is the raw struct bytes.
		auto* in = pins.GetPinValue<nos::fb::Transform>(NOS_NAME("In"));
		auto source = *pins.GetPinValue<nos::track::Frame>(NOS_NAME("SourceFrame"));
		auto target = *pins.GetPinValue<nos::track::Frame>(NOS_NAME("TargetFrame"));

		if (!nos::track::CoordinateFrameValid(source) || !nos::track::CoordinateFrameValid(target))
		{
			nosEngine.LogE("ConvertTransform: Source/Target frame invalid (forward and up must be on different axes)");
			nosEngine.SetPinValue(pins[NOS_NAME("Out")].Id, nos::Buffer::From(*in));
			return NOS_RESULT_SUCCESS;
		}

		const nos::track::FrameConvert conv = nos::track::MakeFrameConvert(source, target);

		// Position: basis change, then unit conversion derived from the two systems.
		const auto& p = in->position();
		glm::dvec3 outPos = nos::track::ConvertPosition(conv, glm::dvec3(p.x(), p.y(), p.z()));

		// Rotation: decode source Euler -> matrix, conjugate by M, re-encode as
		// target Euler. The encodings carry the per-frame Euler order/signs.
		const auto& r = in->rotation();
		glm::dmat3 R_src = nos::track::EulerToMat(source.euler(), glm::dvec3(r.x(), r.y(), r.z()));
		glm::dvec3 outRot = nos::track::MatToEuler(target.euler(), nos::track::ConvertRotation(conv, R_src));

		// Scale: M is a signed axis permutation, so the per-axis factors just reorder.
		const auto& s = in->scale();
		glm::dvec3 outScale = nos::track::ConvertScale(conv, glm::dvec3(s.x(), s.y(), s.z()));

		nos::fb::Transform out(
			nos::fb::vec3d(outPos.x, outPos.y, outPos.z),
			nos::fb::vec3d(outRot.x, outRot.y, outRot.z),
			nos::fb::vec3d(outScale.x, outScale.y, outScale.z));

		nosEngine.SetPinValue(pins[NOS_NAME("Out")].Id, nos::Buffer::From(out));
		return NOS_RESULT_SUCCESS;
	};
}

}  // namespace nos::track
