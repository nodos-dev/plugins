// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
// Frame-conversion helpers for nos.graphics.CoordinateFrame, shared by the
// graphics nodes (ConvertCoordinateFrame / TrackToTransformQ / CameraGuide),
// the Track nodes (ConvertTrackFrame / RecordTrackCOLMAP / PlaybackTrackCOLMAP /
// ConvertTransform) and transform producers such as nos.geometry's FBX reader.
// Builds basis-change matrices from a CoordinateFrame's fields, encodes the
// per-system Euler conventions, and converts to/from the COLMAP camera/world frame.
#pragma once

#include <nosTrack/Coordinates_generated.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

namespace nos::track
{

using Frame = CoordinateFrame;

// Euler encodings, used as the nested `euler` of the frame presets below.
inline EulerEncoding const UNREAL_EULER{EulerOrder::ZYX, -1, -1, 1};
inline EulerEncoding const OPENGL_EULER{EulerOrder::YXZ, 1, 1, 1};

// Named frame presets matching CoordinateSystemPresets.json. COLMAP_SYSTEM is the
// COLMAP camera/world frame (X right, Y down, Z forward, RH, meters); its basis
// equals ColmapBasisMatrix() and it supplies meters_per_unit = 1.0 for unit conversion.
inline CoordinateFrame const UNREAL_SYSTEM{SignedAxis::PosZ, SignedAxis::PosX, Handedness::LeftHanded,  UNREAL_EULER, 0.01};
inline CoordinateFrame const GLTF_SYSTEM  {SignedAxis::PosY, SignedAxis::NegZ, Handedness::RightHanded, OPENGL_EULER, 1.0};
inline CoordinateFrame const COLMAP_SYSTEM{SignedAxis::NegY, SignedAxis::PosZ, Handedness::RightHanded, OPENGL_EULER, 1.0};

// A frame is usable iff forward and up name different axes; otherwise right =
// forward x up is zero and BasisMatrix is singular (inverse -> NaN). Consumers
// should reject invalid frames with a node error rather than emit NaN.
inline bool CoordinateFrameValid(CoordinateFrame const& cs)
{
	// SignedAxis packs as {PosX,NegX,PosY,NegY,PosZ,NegZ}; /2 collapses sign to axis.
	return (static_cast<int>(cs.forward()) / 2) != (static_cast<int>(cs.up()) / 2);
}

// Unit-conversion factor for a length expressed in `src` re-expressed in `tgt`:
// src.meters_per_unit / tgt.meters_per_unit. Guards a non-positive source or
// target scale (zero / negative / NaN) by returning 1.0 (no scaling, no mirrored
// or collapsed world).
inline double UnitFactor(CoordinateFrame const& src, CoordinateFrame const& tgt)
{
	double s = src.meters_per_unit();
	double t = tgt.meters_per_unit();
	if (!(s > 0.0) || !(t > 0.0))
		return 1.0;
	return s / t;
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

// Basis matrix S for a CoordinateFrame: maps semantic (forward, right, up) to
// engine coords. v_engine = S * (forward, right, up). Columns are (forward, right,
// up); `right` is derived as forward x up, flipped for left-handed systems.
// det(S) > 0 for left-handed frames, < 0 for right-handed (with this ordering).
inline glm::dmat3 BasisMatrix(CoordinateFrame const& cs)
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

// Build a rotation matrix from Euler degrees per an EulerEncoding. Component i
// contributes (sign_i * angle_i) about axis i, applied in `order` (intrinsic).
// Independent of any frame's axis geometry -- it only decodes the three numbers.
inline glm::dmat3 EulerToMat(EulerEncoding const& enc, glm::dvec3 const& degRot)
{
	glm::dvec3 r = glm::radians(degRot);
	double x = enc.sign_x() * r.x;
	double y = enc.sign_y() * r.y;
	double z = enc.sign_z() * r.z;
	switch (enc.order())
	{
	case EulerOrder::ZYX: return glm::dmat3(glm::eulerAngleZYX<double>(z, y, x));
	case EulerOrder::XYZ: return glm::dmat3(glm::eulerAngleXYZ<double>(x, y, z));
	case EulerOrder::YXZ: return glm::dmat3(glm::eulerAngleYXZ<double>(y, x, z));
	case EulerOrder::YZX: return glm::dmat3(glm::eulerAngleYZX<double>(y, z, x));
	case EulerOrder::ZXY: return glm::dmat3(glm::eulerAngleZXY<double>(z, x, y));
	case EulerOrder::XZY: return glm::dmat3(glm::eulerAngleXZY<double>(x, z, y));
	}
	return glm::dmat3(1.0);
}

// Inverse of EulerToMat: extract Euler degrees in `enc`'s convention, packed into
// the (rot.x, rot.y, rot.z) = (angle about X, Y, Z) layout. glm::extractEulerAngleABC
// yields the angles in the same A,B,C axis order its name lists; we scatter them
// back to (x, y, z) and undo the per-axis signs (sign in {-1,+1} so multiply).
inline glm::dvec3 MatToEuler(EulerEncoding const& enc, glm::dmat3 const& R)
{
	glm::dmat4 M(R);
	double t1 = 0.0, t2 = 0.0, t3 = 0.0;
	glm::dvec3 ang(0.0);  // ang.x about X, ang.y about Y, ang.z about Z
	switch (enc.order())
	{
	case EulerOrder::ZYX: glm::extractEulerAngleZYX(M, t1, t2, t3); ang = {t3, t2, t1}; break;
	case EulerOrder::XYZ: glm::extractEulerAngleXYZ(M, t1, t2, t3); ang = {t1, t2, t3}; break;
	case EulerOrder::YXZ: glm::extractEulerAngleYXZ(M, t1, t2, t3); ang = {t2, t1, t3}; break;
	case EulerOrder::YZX: glm::extractEulerAngleYZX(M, t1, t2, t3); ang = {t3, t1, t2}; break;
	case EulerOrder::ZXY: glm::extractEulerAngleZXY(M, t1, t2, t3); ang = {t2, t3, t1}; break;
	case EulerOrder::XZY: glm::extractEulerAngleXZY(M, t1, t2, t3); ang = {t1, t3, t2}; break;
	}
	ang.x *= enc.sign_x();
	ang.y *= enc.sign_y();
	ang.z *= enc.sign_z();
	return glm::degrees(ang);
}

// Basis-change M from `cs` to COLMAP frame: M = S_colmap * S_cs^-1.
// For a vector:           v_colmap = M * v_cs.
// For a rotation matrix:  R_colmap = M * R_cs * M^-1.
inline glm::dmat3 BasisChangeToColmap(CoordinateFrame const& cs)
{
	return ColmapBasisMatrix() * glm::inverse(BasisMatrix(cs));
}

// Inverse of BasisChangeToColmap.
inline glm::dmat3 BasisChangeFromColmap(CoordinateFrame const& cs)
{
	return BasisMatrix(cs) * glm::inverse(ColmapBasisMatrix());
}

// --- Shared geometric conversion core -----------------------------------------
// One frame-to-frame conversion, precomputed: the basis change M (maps a vector
// from `src` engine coords to `tgt` engine coords) and the position unit factor.
// All three Convert* nodes build one of these and apply the helpers below, so the
// geometry lives in exactly one place. Rotation handling (Euler vs quaternion) is
// the only thing the nodes do differently.
struct FrameConvert
{
	glm::dmat3 M{1.0};
	double UnitFactor = 1.0;
};

inline FrameConvert MakeFrameConvert(CoordinateFrame const& src, CoordinateFrame const& tgt)
{
	FrameConvert f;
	f.M = BasisMatrix(tgt) * glm::inverse(BasisMatrix(src));
	f.UnitFactor = nos::track::UnitFactor(src, tgt);
	return f;
}

// Position: basis change, then unit conversion.
inline glm::dvec3 ConvertPosition(FrameConvert const& f, glm::dvec3 const& p)
{
	return f.M * p * f.UnitFactor;
}

// Rotation: conjugate by M (orthogonal => M^-1 = M^T). Preserves det(R) = 1, so a
// handedness-flipping (improper) M still yields a proper rotation.
inline glm::dmat3 ConvertRotation(FrameConvert const& f, glm::dmat3 const& R)
{
	return f.M * R * glm::transpose(f.M);
}

inline glm::dquat ConvertRotation(FrameConvert const& f, glm::dquat const& q)
{
	return glm::normalize(glm::quat_cast(ConvertRotation(f, glm::mat3_cast(q))));
}

// Scale: M is a signed axis permutation, so the per-axis factors just reorder.
inline glm::dvec3 ConvertScale(FrameConvert const& f, glm::dvec3 const& s)
{
	glm::dmat3 absM(0.0);
	for (int c = 0; c < 3; ++c)
		for (int row = 0; row < 3; ++row)
			absM[c][row] = std::abs(f.M[c][row]);
	return absM * s;
}

}  // namespace nos::track
