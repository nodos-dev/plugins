// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <Nodos/Helpers.hpp>

#include <Builtins_generated.h>
#include <Graphics_generated.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

// Shared CoordinateFrame basis matrices.
#include <nosGraphics/CoordinateFrameConversion.hpp>

namespace nos::graphics
{
NOS_REGISTER_NAME(In)
NOS_REGISTER_NAME(SourceFrame)
NOS_REGISTER_NAME(TargetFrame)
NOS_REGISTER_NAME(Out)

// Converts a nos.graphics.TransformQ between coordinate frames. Mirrors
// nos.track.ConvertTransform, but rotation is carried as a quaternion so no
// Euler convention is involved: the quaternion is conjugated by the basis-change
// matrix directly.
struct ConvertCoordinateFrameNode : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		nos::NodeExecuteParams pins(params);

		auto* in = pins.GetPinData<TransformQ>(NSN_In);
		auto source = *pins.GetPinData<Frame>(NSN_SourceFrame);
		auto target = *pins.GetPinData<Frame>(NSN_TargetFrame);

		const glm::dmat3 S_src = BasisMatrix(source);
		const glm::dmat3 S_tgt = BasisMatrix(target);
		const glm::dmat3 M = S_tgt * glm::inverse(S_src);

		// Position: basis change, then unit conversion derived from the two systems.
		const auto& p = in->position();
		glm::dvec3 outPos = M * glm::dvec3(p.x(), p.y(), p.z()) * UnitFactor(source, target);

		// Rotation: quaternion -> matrix, conjugate by M (orthogonal => M^-1 = M^T).
		// The conjugation preserves det(R) = 1, so the result stays a proper
		// rotation even when M is a handedness-flipping (improper) basis change.
		const auto& r = in->rotation();
		glm::dquat q = glm::normalize(glm::dquat(r.w(), r.x(), r.y(), r.z()));
		glm::dmat3 R_tgt = M * glm::mat3_cast(q) * glm::transpose(M);
		glm::dquat outQ = glm::normalize(glm::quat_cast(R_tgt));

		// Scale: M is a signed axis permutation, so the per-axis factors reorder.
		const auto& s = in->scale();
		glm::dmat3 absM(0.0);
		for (int c = 0; c < 3; ++c)
			for (int row = 0; row < 3; ++row)
				absM[c][row] = std::abs(M[c][row]);
		glm::dvec3 outScale = absM * glm::dvec3(s.x(), s.y(), s.z());

		TransformQ out(
			fb::vec3d(outPos.x, outPos.y, outPos.z),
			fb::vec4d(outQ.x, outQ.y, outQ.z, outQ.w),
			fb::vec3d(outScale.x, outScale.y, outScale.z));

		SetPinValue(NSN_Out, nos::Buffer::From(out));
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterConvertCoordinateFrame(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.graphics.ConvertCoordinateFrame"), ConvertCoordinateFrameNode, fn);
	return NOS_RESULT_SUCCESS;
}

}  // namespace nos::graphics
