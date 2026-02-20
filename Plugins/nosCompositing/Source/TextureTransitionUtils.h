// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "nosCompositing/Transition_generated.h"

namespace nos::compositing
{

	float InterpStep(float A, float B, float Alpha, uint32_t Steps);

	float InterpEaseIn(float A, float B, float Alpha, float Exp);

	float InterpEaseOut(float A, float B, float Alpha, float Exp);

	float InterpEaseInOut(float A, float B, float Alpha, float Exp);

	float GetInterpolation(TransitionInterpolation interpType, float amount, uint32_t stepCount, float easeExponent);
}