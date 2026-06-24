// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <nosMath/Math_generated.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace nos::math
{

struct QuaternionMultiplyNodeContext : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(nos::NodeExecuteParams const& params) override
	{
		auto* a = params.GetPinValue<fb::vec4d>(NOS_NAME("A"));
		auto* b = params.GetPinValue<fb::vec4d>(NOS_NAME("B"));

		glm::dquat qa(a->w(), a->x(), a->y(), a->z());
		glm::dquat qb(b->w(), b->x(), b->y(), b->z());
		glm::dquat qr = qa * qb;

		nos::fb::vec4d out(qr.x, qr.y, qr.z, qr.w);
		SetPinValue(NOS_NAME("Result"), out);
		return NOS_RESULT_SUCCESS;
	}
};

void RegisterQuaternionMultiply(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.math.QuaternionMultiply"), QuaternionMultiplyNodeContext, fn)
}

}  // namespace nos::math
