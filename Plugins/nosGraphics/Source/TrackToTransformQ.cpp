// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <Nodos/Plugin.hpp>

#include <nosTrack/Track_generated.h>
#include <Builtins_generated.h>
#include <nosTrack/Coordinates_generated.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Track.rotation is Euler degrees in a frame-specific convention; EulerToMat
// turns it into a rotation matrix.
#include <nosTrack/CoordinateFrameConversion.hpp>

namespace nos::graphics
{
// CoordinateFrame/TransformQ types + EulerToMat() live in nos.track (shared, to
// avoid a nos.graphics<->nos.track cycle).
using namespace nos::track;

NOS_REGISTER_NAME(Track)
NOS_REGISTER_NAME(Frame)
NOS_REGISTER_NAME(Out)

// Converts a nos.sys.track.Track's location + Euler rotation into a
// nos.graphics.TransformQ. The Euler angles are interpreted per Frame's euler
// encoding and emitted as a quaternion; no frame conversion is performed, so the
// output is expressed in Frame. Track carries no scale, so scale is identity.
struct TrackToTransformQNode : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(nos::NodeExecuteParams const& pins) override
	{

		nos::track::TTrack const& track = pins.GetPinValue<nos::track::TTrack>(NSN_Track);
		auto frame = *pins.GetPinValue<Frame>(NSN_Frame);

		auto const& loc = track.location;
		auto const& rot = track.rotation;

		glm::dmat3 R = EulerToMat(frame.euler(), glm::dvec3(rot.x(), rot.y(), rot.z()));
		glm::dquat q = glm::normalize(glm::quat_cast(R));

		TransformQ out(
			fb::vec3d(loc.x(), loc.y(), loc.z()),
			fb::vec4d(q.x, q.y, q.z, q.w),
			fb::vec3d(1.0, 1.0, 1.0));

		SetPinValue(NSN_Out, nos::Buffer::From(out));
		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterTrackToTransformQ(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.graphics.TrackToTransformQ"), TrackToTransformQNode, fn);
	return NOS_RESULT_SUCCESS;
}

}  // namespace nos::graphics
