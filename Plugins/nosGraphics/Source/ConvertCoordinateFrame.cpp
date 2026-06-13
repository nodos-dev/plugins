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

		if (!CoordinateSystemValid(source) || !CoordinateSystemValid(target))
		{
			SetNodeStatusMessage("Source/Target frame is invalid: forward and up must be on different axes",
								 fb::NodeStatusMessageType::FAILURE);
			return NOS_RESULT_FAILED;
		}

		const FrameConvert conv = MakeFrameConvert(source, target);

		const auto& p = in->position();
		glm::dvec3 outPos = ConvertPosition(conv, glm::dvec3(p.x(), p.y(), p.z()));

		// Rotation carried as a quaternion, so no Euler convention is involved: the
		// quaternion is conjugated by the basis-change matrix directly.
		const auto& r = in->rotation();
		glm::dquat q = glm::normalize(glm::dquat(r.w(), r.x(), r.y(), r.z()));
		glm::dquat outQ = ConvertRotation(conv, q);

		const auto& s = in->scale();
		glm::dvec3 outScale = ConvertScale(conv, glm::dvec3(s.x(), s.y(), s.z()));

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
