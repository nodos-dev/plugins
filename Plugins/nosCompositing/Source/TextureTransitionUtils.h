// Copyright Zero Density AS. All Rights Reserved.

#include "Common.h"
#include "nosCompositing/Transition_generated.h"

namespace nos::compositing
{

	float InterpStep(float A, float B, float Alpha, u32 Steps);

	float InterpEaseIn(float A, float B, float Alpha, float Exp);

	float InterpEaseOut(float A, float B, float Alpha, float Exp);

	float InterpEaseInOut(float A, float B, float Alpha, float Exp);

	float GetInterpolation(TransitionInterpolation interpType, float amount, u32 stepCount, float easeExponent);
}