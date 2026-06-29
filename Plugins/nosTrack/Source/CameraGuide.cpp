// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

#include <nosTrack/Track_generated.h>
#include <nosTrack/Guidance_generated.h>
#include <Builtins_generated.h>
#include <nosMath/Math_generated.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <string>

// Shared CoordinateFrame basis matrices + Euler extraction.
#include <nosMath/CoordinateFrameConversion.hpp>

namespace nos::track
{
// CoordinateFrame + conversion helpers live in nos.math; guidance types are local
// to nos.track. One-way (acyclic) dependency: nos.track -> nos.math.
using namespace nos::math;

NOS_REGISTER_NAME(Source)
NOS_REGISTER_NAME(Target)
NOS_REGISTER_NAME(Frame)
NOS_REGISTER_NAME(PositionTolerance)
NOS_REGISTER_NAME(AngleTolerance)
NOS_REGISTER_NAME(Out)
NOS_REGISTER_NAME(Guidance)
NOS_REGISTER_NAME(GuidanceText)

using GuideFrame = Frame;

enum class GuideLevel : uint8_t
{
	Aligned,         // within tolerance -> INFO
	NeedsCorrection, // a move/turn is required -> WARNING
};

// Snapshot of one evaluation. ExecuteNode is deterministic in its inputs, so two
// equal snapshots produce an identical status message; comparing snapshots
// (cheap value compare) lets us re-emit the status only when it actually changes
// instead of every frame.
struct GuideState
{
	GuideLevel Level = GuideLevel::Aligned;
	double Fwd = 0.0, Right = 0.0, Up = 0.0;
	double Pan = 0.0, Tilt = 0.0, Roll = 0.0;
	double PosTol = 0.0, AngTol = 0.0;
	bool operator==(GuideState const&) const = default;
};

// Describes the egocentric motion that turns Source into Target: translation
// along Source's local axes (forward/right/up of the chosen frame) and rotation
// about them (pan = yaw about up, tilt = pitch about right, roll about forward).
// Source is passed straight through to Out (bypass).
struct CameraGuideNode : NodeContext
{
	using NodeContext::NodeContext;

	// Last evaluated state; status is pushed only when this changes.
	std::optional<GuideState> LastState;

	nosResult ExecuteNode(nos::NodeExecuteParams const& pins) override
	{

		nos::track::TTrack const& src = pins.GetPinValue<nos::track::TTrack>(NSN_Source);
		nos::track::TTrack const& tgt = pins.GetPinValue<nos::track::TTrack>(NSN_Target);
		auto frame = *pins.GetPinValue<GuideFrame>(NSN_Frame);
		double posTol = *pins.GetPinValue<float>(NSN_PositionTolerance);
		double angTol = *pins.GetPinValue<float>(NSN_AngleTolerance);

		// Bypass: Source -> Out, unchanged.
		SetPinValue(NSN_Out, nos::Buffer::From(src));

		// Both Tracks carry Euler rotation in the same Frame, so they are decoded
		// the same way -- no cross-frame mismatch is possible.
		auto const& sr = src.rotation;
		auto const& tr = tgt.rotation;
		glm::dmat3 R_src = EulerToMat(frame.euler(), glm::dvec3(sr.x(), sr.y(), sr.z()));
		glm::dmat3 R_tgt = EulerToMat(frame.euler(), glm::dvec3(tr.x(), tr.y(), tr.z()));

		auto const& sp = src.location;
		auto const& tp = tgt.location;
		glm::dvec3 dWorld(tp.x() - sp.x(), tp.y() - sp.y(), tp.z() - sp.z());

		// Express the positional delta in Source's local axes (egocentric).
		glm::dvec3 dLocal = glm::transpose(R_src) * dWorld;

		// Semantic axes (engine-space columns of S): 0 forward, 1 right, 2 up.
		glm::dmat3 S = BasisMatrix(frame);
		double fwd = glm::dot(dLocal, glm::dvec3(S[0]));
		double right = glm::dot(dLocal, glm::dvec3(S[1]));
		double up = glm::dot(dLocal, glm::dvec3(S[2]));

		// Rotation guidance in camera-operator terms, taken against the frame's
		// vertical (world up) rather than Source's local up. This is what keeps
		// "looking around" honest: pan is a rotation about world up, tilt is the
		// change in elevation, roll is the change in bank about forward. As long
		// as both up-vectors stay vertical (no banking), roll comes out zero.
		glm::dvec3 F0(S[0]); // forward axis
		glm::dvec3 U0(S[2]); // up axis (vertical / pan axis)
		glm::dvec3 fs = glm::normalize(R_src * F0);
		glm::dvec3 ft = glm::normalize(R_tgt * F0);

		auto elevation = [&](glm::dvec3 const& f) {
			return std::asin(glm::clamp(glm::dot(f, U0), -1.0, 1.0));
		};
		// Bank: signed angle about forward between the actual up and the
		// zero-roll up for that forward direction.
		auto bank = [&](glm::dmat3 const& R) {
			glm::dvec3 f = glm::normalize(R * F0);
			glm::dvec3 r = glm::cross(f, U0);
			double rl = glm::length(r);
			if (rl < 1e-9) // looking straight up/down: bank is undefined
				return 0.0;
			r /= rl;
			glm::dvec3 upRef = glm::cross(r, f); // zero-roll up for this forward
			glm::dvec3 upR = R * U0;
			return std::atan2(glm::dot(glm::cross(upRef, upR), f), glm::dot(upRef, upR));
		};

		// Pan: angle between the horizontal headings. The left/right side is
		// decided by which side of Source's (horizontal) right axis the Target
		// heading falls on -- a spatial test, so it stays correct in both left-
		// and right-handed frames (a rotation-sign convention would not).
		glm::dvec3 fsH = fs - glm::dot(fs, U0) * U0;
		glm::dvec3 ftH = ft - glm::dot(ft, U0) * U0;
		double pan = 0.0;
		if (glm::length(fsH) > 1e-9 && glm::length(ftH) > 1e-9)
		{
			fsH = glm::normalize(fsH);
			ftH = glm::normalize(ftH);
			glm::dvec3 rs = R_src * glm::dvec3(S[1]); // Source right axis
			rs = rs - glm::dot(rs, U0) * U0;          // horizontal component
			double mag = std::acos(glm::clamp(glm::dot(fsH, ftH), -1.0, 1.0));
			double side = glm::dot(ftH, rs); // > 0: Target heading is to the right
			pan = side >= 0.0 ? mag : -mag;  // pan > 0 -> Pan right
		}
		double tilt = elevation(ft) - elevation(fs);
		double roll = bank(R_tgt) - bank(R_src);
		pan = glm::degrees(pan);
		tilt = glm::degrees(tilt);
		roll = glm::degrees(roll);

		bool needsMove = std::abs(fwd) >= posTol || std::abs(right) >= posTol || std::abs(up) >= posTol;
		bool needsTurn = std::abs(pan) >= angTol || std::abs(tilt) >= angTol || std::abs(roll) >= angTol;

		GuideState state{
			needsMove || needsTurn ? GuideLevel::NeedsCorrection : GuideLevel::Aligned,
			fwd, right, up, pan, tilt, roll, posTol, angTol};

		// Nothing changed since last frame -> don't re-emit the status.
		if (LastState == state)
			return NOS_RESULT_SUCCESS;
		LastState = state;

		std::string msg;
		auto move = [&](double v, const char* pos, const char* neg) {
			if (std::abs(v) < posTol)
				return;
			if (!msg.empty())
				msg += ", ";
			msg += std::format("{} {:.2f}", v >= 0 ? pos : neg, std::abs(v));
		};
		move(fwd, "Forward", "Back");
		move(right, "Right", "Left");
		move(up, "Up", "Down");

		std::string rot;
		auto turn = [&](double deg, const char* pos, const char* neg) {
			if (std::abs(deg) < angTol)
				return;
			if (!rot.empty())
				rot += ", ";
			rot += std::format("{} {:.1f} deg", deg >= 0 ? pos : neg, std::abs(deg));
		};
		turn(pan, "Pan right", "Pan left");
		turn(tilt, "Tilt up", "Tilt down");
		// roll is a right-hand rotation about +forward; the operator looks along
		// +forward, so positive reads as counter-clockwise.
		turn(roll, "Roll CCW", "Roll CW");

		std::string status;
		if (state.Level == GuideLevel::Aligned)
			status = "Aligned (within tolerance)";
		else
		{
			if (!msg.empty())
				status = "Move: " + msg;
			if (!rot.empty())
				status += (status.empty() ? "" : "\n") + std::string("Rotate: ") + rot;
		}
		SetNodeStatusMessage(status, state.Level == GuideLevel::Aligned
										 ? fb::NodeStatusMessageType::INFO
										 : fb::NodeStatusMessageType::WARNING);

		// Structured guidance on an output pin; compose display text downstream.
		TransformGuidance guidance(
			state.Level == GuideLevel::Aligned ? AlignmentState::Aligned
											   : AlignmentState::NeedsCorrection,
			(float)fwd, (float)right, (float)up, (float)pan, (float)tilt, (float)roll);
		SetPinValue(NSN_Guidance, nos::Buffer::From(guidance));

		// Same text as the node status, for a downstream text renderer.
		SetPinValue(NSN_GuidanceText, nos::Buffer(status.c_str(), status.size() + 1));

		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterCameraGuide(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.track.CameraGuide"), CameraGuideNode, fn);
	return NOS_RESULT_SUCCESS;
}

}  // namespace nos::track
