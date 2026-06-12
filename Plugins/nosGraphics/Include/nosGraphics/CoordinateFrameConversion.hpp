// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
// Frame-conversion helpers for nos.graphics.CoordinateSystem, shared by the
// graphics nodes (ConvertCoordinateFrame / TrackToTransformQ / CameraGuide),
// the Track nodes (ConvertTrackFrame / RecordTrackCOLMAP / PlaybackTrackCOLMAP /
// ConvertTransform) and transform producers such as nos.geometry's FBX reader.
// Builds basis-change matrices from a CoordinateSystem's fields, encodes the
// per-system Euler conventions, and converts to/from the COLMAP camera/world frame.
#pragma once

#include <Graphics_generated.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace nos::graphics
{

using Frame = CoordinateSystem;

// Named presets matching CoordinateSystemPresets.json. COLMAP_SYSTEM is the COLMAP
// camera/world frame (X right, Y down, Z forward, RH, meters); its basis equals
// ColmapBasisMatrix() and it supplies meters_per_unit = 1.0 for unit conversion.
inline CoordinateSystem const UNREAL_SYSTEM{SignedAxis::PosZ, SignedAxis::PosX, Handedness::LeftHanded,  RotationConvention::UnrealZYX, 0.01};
inline CoordinateSystem const GLTF_SYSTEM  {SignedAxis::PosY, SignedAxis::NegZ, Handedness::RightHanded, RotationConvention::OpenGLYXZ, 1.0};
inline CoordinateSystem const COLMAP_SYSTEM{SignedAxis::NegY, SignedAxis::PosZ, Handedness::RightHanded, RotationConvention::OpenGLYXZ, 1.0};

// Unit-conversion factor for a length expressed in `src` re-expressed in `tgt`:
// src.meters_per_unit / tgt.meters_per_unit. Guards a non-positive target scale
// (zero / negative / NaN) by returning 1.0 (no scaling, no mirrored world).
inline double UnitFactor(CoordinateSystem const& src, CoordinateSystem const& tgt)
{
	double t = tgt.meters_per_unit();
	if (!(t > 0.0))
		return 1.0;
	return src.meters_per_unit() / t;
}

// Unit vector for a signed coordinate axis.
inline glm::dvec3 AxisVec(SignedAxis a)
{
	switch (a)
	{
	case SignedAxis::PosX: return glm::dvec3( 1.0,  0.0,  0.0);
	case SignedAxis::NegX: return glm::dvec3(-1.0,  0.0,  0.0);
	case SignedAxis::PosY: return glm::dvec3( 0.0,  1.0,  0.0);
	case SignedAxis::NegY: return glm::dvec3( 0.0, -1.0,  0.0);
	case SignedAxis::PosZ: return glm::dvec3( 0.0,  0.0,  1.0);
	case SignedAxis::NegZ: return glm::dvec3( 0.0,  0.0, -1.0);
	}
	return glm::dvec3(0.0);
}

// Basis matrix S for a CoordinateSystem: maps semantic (forward, right, up) to
// engine coords. v_engine = S * (forward, right, up). Columns are (forward, right,
// up); `right` is derived as forward x up, flipped for left-handed systems.
// det(S) > 0 for left-handed frames, < 0 for right-handed (with this ordering).
inline glm::dmat3 BasisMatrix(CoordinateSystem const& cs)
{
	glm::dvec3 fwd = AxisVec(cs.forward());
	glm::dvec3 up = AxisVec(cs.up());
	glm::dvec3 right = glm::cross(fwd, up);
	if (cs.handedness() == Handedness::LeftHanded)
		right = -right;
	return glm::dmat3(fwd, right, up);
}

// COLMAP camera/world frame: X right, Y down, Z forward (RH). Provided as a basis
// matrix in the same (forward, right, up) convention so it can be combined with
// BasisMatrix to build cross-frame conversions.
inline glm::dmat3 ColmapBasisMatrix()
{
	return glm::dmat3(
		glm::dvec3( 0.0,  0.0,  1.0),  // forward -> +Z
		glm::dvec3( 1.0,  0.0,  0.0),  // right -> +X
		glm::dvec3( 0.0, -1.0,  0.0)); // up -> -Y (Y is down)
}

// Build R_c2w in `cs` from rotation Euler degrees, per the system's Euler convention.
inline glm::dmat3 EulerToMat(CoordinateSystem const& cs, glm::dvec3 const& degRot)
{
	glm::dvec3 r = glm::radians(degRot);
	switch (cs.rotation())
	{
	case RotationConvention::UnrealZYX:
		// FRotator: rot.x = roll, rot.y = pitch, rot.z = yaw, intrinsic ZYX.
		// UE sign convention has +pitch = look up and +roll = bank right via
		// LH-rule rotations, equivalent to standard-RH Rz(yaw) * Ry(-pitch) * Rx(-roll).
		return glm::dmat3(glm::eulerAngleZYX<double>(r.z, -r.y, -r.x));
	case RotationConvention::OpenGLYXZ:
		// rot.x = pitch, rot.y = yaw, rot.z = roll, intrinsic YXZ:
		// R = Ry(yaw) * Rx(pitch) * Rz(roll), all standard-RH formulas.
		return glm::dmat3(glm::eulerAngleYXZ<double>(r.y, r.x, r.z));
	}
	return glm::dmat3(1.0);
}

// Inverse of EulerToMat: extract Euler degrees in `cs`'s convention, packed into
// the (rot.x, rot.y, rot.z) layout for that convention.
inline glm::dvec3 MatToEuler(CoordinateSystem const& cs, glm::dmat3 const& R)
{
	glm::dmat4 M(R);
	double a = 0.0, b = 0.0, c = 0.0;
	switch (cs.rotation())
	{
	case RotationConvention::UnrealZYX:
		glm::extractEulerAngleZYX(M, a, b, c);  // a=yaw, b=pitch, c=roll
		// Negate pitch and roll back to UE sign convention; pack as (roll, pitch, yaw).
		return glm::degrees(glm::dvec3(-c, -b, a));
	case RotationConvention::OpenGLYXZ:
		glm::extractEulerAngleYXZ(M, a, b, c);  // a=yaw, b=pitch, c=roll
		// Pack as (pitch, yaw, roll).
		return glm::degrees(glm::dvec3(b, a, c));
	}
	return glm::dvec3(0.0);
}

// Basis-change M from `cs` to COLMAP frame: M = S_colmap * S_cs^-1.
// For a vector:           v_colmap = M * v_cs.
// For a rotation matrix:  R_colmap = M * R_cs * M^-1.
inline glm::dmat3 BasisChangeToColmap(CoordinateSystem const& cs)
{
	return ColmapBasisMatrix() * glm::inverse(BasisMatrix(cs));
}

// Inverse of BasisChangeToColmap.
inline glm::dmat3 BasisChangeFromColmap(CoordinateSystem const& cs)
{
	return BasisMatrix(cs) * glm::inverse(ColmapBasisMatrix());
}

}  // namespace nos::graphics
