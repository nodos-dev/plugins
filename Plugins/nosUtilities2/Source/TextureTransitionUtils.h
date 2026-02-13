// Copyright Zero Density AS. All Rights Reserved.

#include "Common.h"

namespace nos
{
	enum class EZDTransitionInterpolation {
		Jump,
		Linear,
		Step,
		EaseIn,
		EaseOut,
		EaseInOut
	};

	float InterpStep(float A, float B, float Alpha, u32 Steps);

	float InterpEaseIn(float A, float B, float Alpha, float Exp);

	float InterpEaseOut(float A, float B, float Alpha, float Exp);

	float InterpEaseInOut(float A, float B, float Alpha, float Exp);

	float GetInterpolation(EZDTransitionInterpolation interpType, float amount, u32 stepCount, float easeExponent);
}