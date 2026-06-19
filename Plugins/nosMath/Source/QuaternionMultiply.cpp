// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <Math_generated.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace nos::math
{

struct QuaternionMultiplyNodeContext : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(nosNodeExecuteParams* execParams) override
	{
		nos::NodeExecuteParams params(execParams);
		auto* a = params.GetPinData<fb::vec4d>(NOS_NAME("A"));
		auto* b = params.GetPinData<fb::vec4d>(NOS_NAME("B"));
		auto* out = params.GetPinData<fb::vec4d>(NOS_NAME("Result"));

		glm::dquat qa(a->w(), a->x(), a->y(), a->z());
		glm::dquat qb(b->w(), b->x(), b->y(), b->z());
		glm::dquat qr = qa * qb;

		out->mutate_x(qr.x);
		out->mutate_y(qr.y);
		out->mutate_z(qr.z);
		out->mutate_w(qr.w);
		return NOS_RESULT_SUCCESS;
	}
};

void RegisterQuaternionMultiply(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.math.QuaternionMultiply"), QuaternionMultiplyNodeContext, fn)
}

}  // namespace nos::math
