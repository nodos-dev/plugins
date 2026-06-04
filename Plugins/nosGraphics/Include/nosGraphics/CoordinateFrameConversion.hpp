// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
// Frame-conversion helpers for nos.graphics.CoordinateFrame, shared by the
// graphics nodes (ConvertCoordinateFrame / TrackToTransformQ / CameraGuide),
// the Track nodes (TrackTransform / RecordTrackCOLMAP / PlaybackTrackCOLMAP /
// ConvertTransform) and transform producers such as nos.geometry's FBX reader.
// Encodes per-frame Euler conventions and basis-change matrices to the COLMAP
// camera/world frame.
#pragma once

#include <Graphics_generated.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace nos::graphics
{

using Frame = CoordinateFrame;

// Basis matrix S for a CoordinateFrame: maps semantic (forward, right, up)
// to engine coords (vx, vy, vz). v_engine = S * (forward, right, up).
// det(S) > 0 for left-handed frames, < 0 for right-handed (with this ordering).
inline glm::dmat3 BasisMatrix(Frame frame)
{
	switch (frame)
	{
	case Frame::LH_ZUp_FwdX_RightY:
		// vx = forward, vy = right, vz = up.
		return glm::dmat3(1.0);
	case Frame::RH_YUp_FwdNegZ_RightX:
		// vx = right, vy = up, vz = -forward.
		return glm::dmat3(
			glm::dvec3( 0.0,  0.0, -1.0),  // M * (1,0,0) = forward column
			glm::dvec3( 1.0,  0.0,  0.0),  // M * (0,1,0) = right column
			glm::dvec3( 0.0,  1.0,  0.0)); // M * (0,0,1) = up column
	}
	return glm::dmat3(1.0);
}

// COLMAP camera/world frame: X right, Y down, Z forward (RH).
// Provided as a basis matrix in the same (forward, right, up) convention so
// it can be combined with BasisMatrix to build cross-frame conversions.
inline glm::dmat3 ColmapBasisMatrix()
{
	return glm::dmat3(
		glm::dvec3( 0.0,  0.0,  1.0),  // forward -> +Z
		glm::dvec3( 1.0,  0.0,  0.0),  // right -> +X
		glm::dvec3( 0.0, -1.0,  0.0)); // up -> -Y (Y is down)
}

// Build R_c2w in `frame` from rotation Euler degrees.
inline glm::dmat3 EulerToMat(Frame frame, glm::dvec3 const& degRot)
{
	glm::dvec3 r = glm::radians(degRot);
	switch (frame)
	{
	case Frame::LH_ZUp_FwdX_RightY:
		// FRotator: rot.x = roll, rot.y = pitch, rot.z = yaw, intrinsic ZYX.
		// UE sign convention has +pitch = look up and +roll = bank right via
		// LH-rule rotations, equivalent to standard-RH Rz(yaw) * Ry(-pitch) * Rx(-roll).
		return glm::dmat3(glm::eulerAngleZYX<double>(r.z, -r.y, -r.x));
	case Frame::RH_YUp_FwdNegZ_RightX:
		// rot.x = pitch, rot.y = yaw, rot.z = roll, intrinsic YXZ:
		// R = Ry(yaw) * Rx(pitch) * Rz(roll), all standard-RH formulas.
		return glm::dmat3(glm::eulerAngleYXZ<double>(r.y, r.x, r.z));
	}
	return glm::dmat3(1.0);
}

// Inverse of EulerToMat: extract Euler degrees in `frame`'s convention.
// Output is packed into the (rot.x, rot.y, rot.z) layout for that frame.
inline glm::dvec3 MatToEuler(Frame frame, glm::dmat3 const& R)
{
	glm::dmat4 M(R);
	double a = 0.0, b = 0.0, c = 0.0;
	switch (frame)
	{
	case Frame::LH_ZUp_FwdX_RightY:
		glm::extractEulerAngleZYX(M, a, b, c);  // a=yaw, b=pitch, c=roll
		// Negate pitch and roll back to UE sign convention; pack as (roll, pitch, yaw).
		return glm::degrees(glm::dvec3(-c, -b, a));
	case Frame::RH_YUp_FwdNegZ_RightX:
		glm::extractEulerAngleYXZ(M, a, b, c);  // a=yaw, b=pitch, c=roll
		// Pack as (pitch, yaw, roll).
		return glm::degrees(glm::dvec3(b, a, c));
	}
	return glm::dvec3(0.0);
}

// Basis-change M from `frame` to COLMAP frame: M = S_colmap * S_frame^-1.
// For a vector:           v_colmap = M * v_frame.
// For a rotation matrix:  R_colmap = M * R_frame * M^-1.
inline glm::dmat3 BasisChangeToColmap(Frame frame)
{
	return ColmapBasisMatrix() * glm::inverse(BasisMatrix(frame));
}

// Inverse of BasisChangeToColmap.
inline glm::dmat3 BasisChangeFromColmap(Frame frame)
{
	return BasisMatrix(frame) * glm::inverse(ColmapBasisMatrix());
}

}  // namespace nos::graphics
