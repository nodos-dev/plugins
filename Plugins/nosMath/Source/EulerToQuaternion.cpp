// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <nosMath/Math_generated.h>
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

	nosResult ExecuteNode(nos::NodeExecuteParams const& params) override
	{
		auto* in = params.GetPinValue<fb::vec3d>(NOS_NAME("Euler"));
		auto* order = params.GetPinValue<EulerOrder>(NOS_NAME("Order"));

		glm::dvec3 r = glm::radians(glm::dvec3(in->x(), in->y(), in->z()));
		glm::dquat q(EulerToMat(*order, r));

		nos::fb::vec4d out(q.x, q.y, q.z, q.w);
		SetPinValue(NOS_NAME("Quaternion"), out);
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

	nosResult ExecuteNode(nos::NodeExecuteParams const& params) override
	{
		auto* in = params.GetPinValue<fb::vec4d>(NOS_NAME("Quaternion"));
		auto* order = params.GetPinValue<EulerOrder>(NOS_NAME("Order"));

		glm::dquat q(in->w(), in->x(), in->y(), in->z());
		glm::dmat4 m = glm::mat4_cast(q);
		glm::dvec3 r(0.0);
		MatToEuler(*order, m, r);

		nos::fb::vec3d out(glm::degrees(r.x), glm::degrees(r.y), glm::degrees(r.z));
		SetPinValue(NOS_NAME("Euler"), out);
		return NOS_RESULT_SUCCESS;
	}
};

void RegisterQuaternionToEuler(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.math.QuaternionToEuler"), QuaternionToEulerNodeContext, fn)
}

}  // namespace nos::math
