// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <nosTrack/Track_generated.h>
#include <Graphics_generated.h>

#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

// Inside Nodos/PluginHelpers.hpp
#define NOS_REGISTER_NODE(NodeName)                                                                                    \
	nosResult Register##NodeName(nosNodeFunctions* fn)                                                                 \
	{                                                                                                                  \
		NOS_BIND_NODE_CLASS(NOS_NAME(#NodeName), NodeName, fn);                                                        \
		return NodeName::OnRegister();                                                                                 \
	}

namespace nos::graphics
{
glm::mat4 MakeView(glm::vec3 pos, glm::vec3 rot)
{
	rot = glm::radians(rot);
	auto mat = (glm::mat3)glm::eulerAngleZYX(rot.z, -rot.y, -rot.x);
	return glm::lookAtLH(pos, pos + mat[0], mat[2]);
}

glm::mat4 Perspective(f32 fovx, f32 pixelAspectRatio, glm::vec2 sensorSize, glm::vec2 centerShift, glm::vec2 clipPlanes)
{
	if (glm::vec2(0) == sensorSize)
	{
		sensorSize = glm::vec2(1);
		centerShift = glm::vec2(0);
	}

	const f32 X = 1.f / tanf(glm::radians(fovx * 0.5f));
	const f32 Y = -X * (sensorSize.x / sensorSize.y) * pixelAspectRatio;
	const auto S = -centerShift / sensorSize;
	const f32 Z = clipPlanes.y / (clipPlanes.y - clipPlanes.x);
	return glm::mat4(glm::vec4(X, 0, 0, 0),
					 glm::vec4(0, Y, 0, 0),
					 glm::vec4(S.x, -S.y, Z, 1.0f),
					 glm::vec4(0, 0, -clipPlanes.x * Z, 0));
}

glm::vec2 CalculateProjectionShift(glm::vec2 sensorSize, glm::vec2 centerShift)
{
	if (centerShift == glm::vec2(0))
		return glm::vec2(0);
	if (sensorSize == glm::vec2(0))
		sensorSize = glm::vec2(1);
	auto projectionShift = glm::vec2(-centerShift / sensorSize);
	projectionShift.y = -projectionShift.y;
	return projectionShift;
}

NOS_REGISTER_NAME(Track)
NOS_REGISTER_NAME(AspectRatio)
NOS_REGISTER_NAME(Clip)
NOS_REGISTER_NAME(View)
struct TrackToView : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		NodeExecuteParams pins(params);
		nos::track::TTrack const& track = pins.GetPinData<nos::track::TTrack>(NSN_Track);
		float aspectRatio = *pins.GetPinData<float>(NSN_AspectRatio);
		nos::fb::vec2 clip = *pins.GetPinData<nos::fb::vec2>(NSN_Clip);

		glm::vec2 sensorSize = reinterpret_cast<glm::vec2 const&>(track.sensor_size);
		glm::vec2 centerShift = reinterpret_cast<glm::vec2 const&>(track.lens_distortion.center_shift());
		glm::vec2 projShift = CalculateProjectionShift(sensorSize, centerShift);

		TPerspectiveProjection perspectiveProjection{};
		perspectiveProjection.aspect_ratio = aspectRatio;
		perspectiveProjection.fov_x = track.fov;
		TProjection projection{};
		projection.clip_planes = clip;
		projection.shift = reinterpret_cast<nos::fb::vec2 const&>(projShift);
		projection.projection_type = ProjectionType::Perspective;
		projection.perspective = std::make_unique<TPerspectiveProjection>(perspectiveProjection);
		glm::mat4 viewMatrix = MakeView(
			reinterpret_cast<glm::vec3 const&>(track.location),
										reinterpret_cast<glm::vec3 const&>(track.rotation));
		glm::mat4 projectionMatrix = Perspective(
			track.fov, track.pixel_aspect_ratio, sensorSize, centerShift, reinterpret_cast<glm::vec2 const&>(clip));

		TRenderView view{};
		view.projection = std::make_unique<TProjection>(projection);
		view.view = reinterpret_cast<nos::fb::mat4 const&>(viewMatrix);
		view.left_handed_projection_matrix = reinterpret_cast<nos::fb::mat4 const&>(projectionMatrix);
		SetPinValue(NSN_View, Buffer::From(view));
		return NOS_RESULT_SUCCESS;
	}

	static nosResult OnRegister() { return NOS_RESULT_SUCCESS; }
};

NOS_REGISTER_NODE(TrackToView)

} // namespace nos::graphics
