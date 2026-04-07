// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/SubsystemAPI.h>
#include <Nodos/Name.hpp>
#include <Nodos/Helpers.hpp>
#include <nosSysTrack/Track_generated.h>
#include <nosAnimationSubsystem/nosAnimationSubsystem.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/ext/quaternion_common.hpp>

NOS_INIT_WITH_MIN_REQUIRED_MINOR(0)
NOS_ANIMATION_INIT()

NOS_BEGIN_IMPORT_DEPS()
NOS_ANIMATION_IMPORT()
NOS_END_IMPORT_DEPS()

namespace nos::sys::track
{
nosResult NOSAPI_CALL OnRequest(uint32_t minorVersion, void** outSubsystemContext)
{
	(void)minorVersion;
	if (outSubsystemContext)
		*outSubsystemContext = nullptr;
	return NOS_RESULT_SUCCESS;
}

void NOSAPI_CALL GetRenamedTypes(nosName* outRenamedFrom, nosName* outRenamedTo, size_t* outSize)
{
	auto trackName = NOS_NAME(nos::sys::track::Track::GetFullyQualifiedName());
	auto lensDistortionName = NOS_NAME(nos::sys::track::LensDistortion::GetFullyQualifiedName());
	auto rotationSystemName = NOS_NAME("nos.sys.track.RotationSystem");
	auto coordinateSystemName = NOS_NAME("nos.sys.track.CoordinateSystem");
	static std::vector<std::pair<nos::Name, nos::Name>> renames = {
		{NOS_NAME("nos.fb.Track"), trackName},
		{NOS_NAME("nos.track.Track"), trackName},
		{NOS_NAME("zd.sys.track.Track"), trackName},
		{NOS_NAME("nos.fb.LensDistortion"), lensDistortionName},
		{NOS_NAME("nos.track.LensDistortion"), lensDistortionName},
		{NOS_NAME("zd.sys.track.LensDistortion"), lensDistortionName},
		{NOS_NAME("nos.fb.CoordinateSystem"), coordinateSystemName},
		{NOS_NAME("nos.track.CoordinateSystem"), coordinateSystemName},
		{NOS_NAME("zd.sys.track.CoordinateSystem"), coordinateSystemName},
		{NOS_NAME("nos.fb.RotationSystem"), rotationSystemName},
		{NOS_NAME("nos.track.RotationSystem"), rotationSystemName},
		{NOS_NAME("zd.sys.track.RotationSystem"), rotationSystemName},
	};

	if (!outRenamedFrom)
	{
		*outSize = renames.size();
		return;
	}

	for (size_t i = 0; i < renames.size(); ++i)
	{
		outRenamedFrom[i] = renames[i].first;
		outRenamedTo[i] = renames[i].second;
	}
}
template <typename T, size_t N>
T LerpVec(const T& from, const T& to, double t)
{
	T result{};
	result.mutate_x(glm::mix(from.x(), to.x(), t));
	result.mutate_y(glm::mix(from.y(), to.y(), t));
	if constexpr (N > 2)
		result.mutate_z(glm::mix(from.z(), to.z(), t));
	return result;
}

template <typename Vec3Type>
Vec3Type InterpolateRotation(const Vec3Type& start, const Vec3Type& end, const double t)
{
	using ScalarType = decltype(start.x());
	glm::vec<3, ScalarType> glmStart(start.x(), start.y(), start.z());
	glm::vec<3, ScalarType> glmEnd(end.x(), end.y(), end.z());
	glm::qua<ScalarType> startQuat = glm::quat(glm::radians(glmStart));
	glm::qua<ScalarType> endQuat = glm::quat(glm::radians(glmEnd));
	glm::qua<ScalarType> newQuat = glm::slerp(startQuat, endQuat, static_cast<ScalarType>(t));
	glm::vec<3, ScalarType> newEuler = glm::degrees(glm::eulerAngles(newQuat));
	return Vec3Type(newEuler.x, newEuler.y, newEuler.z);
}

nosResult NOSAPI_CALL InterpolateTrack(const nosBuffer from, const nosBuffer to, const double t, nosBuffer* out)
{
	auto start = flatbuffers::GetRoot<nos::sys::track::Track>(from.Data);
	auto end = flatbuffers::GetRoot<nos::sys::track::Track>(to.Data);
	nos::sys::track::TTrack interm;
	interm.location = LerpVec<fb::vec3, 3>(*start->location(), *end->location(), t);
	interm.rotation = InterpolateRotation(*start->rotation(), *end->rotation(), t); // quat
	interm.fov = glm::mix(start->fov(), end->fov(), t);
	interm.focus = glm::mix(start->focus(), end->focus(), t);
	interm.zoom = glm::mix(start->zoom(), end->zoom(), t);
	interm.render_ratio = glm::mix(start->render_ratio(), end->render_ratio(), t);
	interm.sensor_size = LerpVec<fb::vec2, 2>(*start->sensor_size(), *end->sensor_size(), t);
	interm.pixel_aspect_ratio = glm::mix(start->pixel_aspect_ratio(), end->pixel_aspect_ratio(), t);
	interm.nodal_offset = glm::mix(start->nodal_offset(), end->nodal_offset(), t);
	interm.focus_distance = glm::mix(start->focus_distance(), end->focus_distance(), t);
	auto distortionStart = *start->lens_distortion();
	auto distortionEnd = *end->lens_distortion();
	auto& distortion = interm.lens_distortion;
	distortion.mutable_center_shift() =
		LerpVec<fb::vec2, 2>(distortionStart.center_shift(), distortionEnd.center_shift(), t);
	distortion.mutable_k1k2() = LerpVec<fb::vec2, 2>(distortionStart.k1k2(), distortionEnd.k1k2(), t);
	distortion.mutate_distortion_scale(
		glm::mix(distortionStart.distortion_scale(), distortionEnd.distortion_scale(), t));

	*out = EngineBuffer::CopyFrom(interm).Release();
	return NOS_RESULT_SUCCESS;
}

nosResult NOSAPI_CALL InterpolateTransform(const nosBuffer from, const nosBuffer to, const double t, nosBuffer* out)
{
	auto start = flatbuffers::GetRoot<fb::Transform>(from.Data);
	auto end = flatbuffers::GetRoot<fb::Transform>(to.Data);
	nos::fb::Transform interm;
	interm.mutable_position() = LerpVec<fb::vec3d, 3>(start->position(), end->position(), t);
	interm.mutable_rotation() = InterpolateRotation(start->rotation(), end->rotation(), t); // quat
	interm.mutable_scale() = LerpVec<fb::vec3d, 3>(start->scale(), end->scale(), t);

	*out = EngineBuffer::CopyFrom(interm).Release();
	return NOS_RESULT_SUCCESS;
}

}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportSubsystem(nosSubsystemFunctions* subsystemFunctions)
{
	subsystemFunctions->OnRequest = nos::sys::track::OnRequest;
	subsystemFunctions->GetRenamedTypes = nos::sys::track::GetRenamedTypes;

	nosAnimationInterpolator trackInterpolator = {.TypeName = NOS_NAME(nos::sys::track::Track::GetFullyQualifiedName()),
												  .InterpolateCallback = nos::sys::track::InterpolateTrack};
	nosAnimation->RegisterInterpolator(&trackInterpolator);

	nosAnimationInterpolator transformInterpolator = {.TypeName = NOS_NAME(nos::fb::Transform::GetFullyQualifiedName()),
													  .InterpolateCallback = nos::sys::track::InterpolateTransform};
	nosAnimation->RegisterInterpolator(&transformInterpolator);
	return NOS_RESULT_SUCCESS;
}
}
