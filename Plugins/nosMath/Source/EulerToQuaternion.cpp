// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <Math_generated.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace nos::math
{

// Build a rotation matrix for the given intrinsic Euler order.
// In all cases, rot.x is the angle about X, rot.y about Y, rot.z about Z (radians).
// Order ZYX means R = Rz(rot.z) * Ry(rot.y) * Rx(rot.x), applied right-to-left to a point.
static glm::dmat4 EulerToMat(EulerOrder order, glm::dvec3 const& r)
{
	switch (order)
	{
	case EulerOrder::ZYX: return glm::eulerAngleZYX<double>(r.z, r.y, r.x);
	case EulerOrder::XYZ: return glm::eulerAngleXYZ<double>(r.x, r.y, r.z);
	case EulerOrder::YXZ: return glm::eulerAngleYXZ<double>(r.y, r.x, r.z);
	case EulerOrder::YZX: return glm::eulerAngleYZX<double>(r.y, r.z, r.x);
	case EulerOrder::ZXY: return glm::eulerAngleZXY<double>(r.z, r.x, r.y);
	case EulerOrder::XZY: return glm::eulerAngleXZY<double>(r.x, r.z, r.y);
	}
	return glm::dmat4(1.0);
}

static void MatToEuler(EulerOrder order, glm::dmat4 const& m, glm::dvec3& r)
{
	switch (order)
	{
	case EulerOrder::ZYX: glm::extractEulerAngleZYX(m, r.z, r.y, r.x); break;
	case EulerOrder::XYZ: glm::extractEulerAngleXYZ(m, r.x, r.y, r.z); break;
	case EulerOrder::YXZ: glm::extractEulerAngleYXZ(m, r.y, r.x, r.z); break;
	case EulerOrder::YZX: glm::extractEulerAngleYZX(m, r.y, r.z, r.x); break;
	case EulerOrder::ZXY: glm::extractEulerAngleZXY(m, r.z, r.x, r.y); break;
	case EulerOrder::XZY: glm::extractEulerAngleXZY(m, r.x, r.z, r.y); break;
	}
}

struct EulerToQuaternionNodeContext : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(nosNodeExecuteParams* execParams) override
	{
		nos::NodeExecuteParams params(execParams);
		auto* in = params.GetPinData<fb::vec3d>(NOS_NAME("Euler"));
		auto* order = params.GetPinData<EulerOrder>(NOS_NAME("Order"));
		auto* out = params.GetPinData<fb::vec4d>(NOS_NAME("Quaternion"));

		glm::dvec3 r = glm::radians(glm::dvec3(in->x(), in->y(), in->z()));
		glm::dquat q(EulerToMat(*order, r));

		out->mutate_x(q.x);
		out->mutate_y(q.y);
		out->mutate_z(q.z);
		out->mutate_w(q.w);
		return NOS_RESULT_SUCCESS;
	}
};

void RegisterEulerToQuaternion(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.math.EulerToQuaternion"), EulerToQuaternionNodeContext, fn)
}

struct QuaternionToEulerNodeContext : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(nosNodeExecuteParams* execParams) override
	{
		nos::NodeExecuteParams params(execParams);
		auto* in = params.GetPinData<fb::vec4d>(NOS_NAME("Quaternion"));
		auto* order = params.GetPinData<EulerOrder>(NOS_NAME("Order"));
		auto* out = params.GetPinData<fb::vec3d>(NOS_NAME("Euler"));

		glm::dquat q(in->w(), in->x(), in->y(), in->z());
		glm::dmat4 m = glm::mat4_cast(q);
		glm::dvec3 r(0.0);
		MatToEuler(*order, m, r);

		out->mutate_x(glm::degrees(r.x));
		out->mutate_y(glm::degrees(r.y));
		out->mutate_z(glm::degrees(r.z));
		return NOS_RESULT_SUCCESS;
	}
};

void RegisterQuaternionToEuler(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.math.QuaternionToEuler"), QuaternionToEulerNodeContext, fn)
}

}  // namespace nos::math
