
#include "TextureTransitionUtils.h"


namespace nos::compositing
{
	float InterpStep(float A, float B, float Alpha, u32 Steps)
	{
		if (Steps <= 1 || Alpha <= 0) {
			return A;
		}
		else if (Alpha >= 1) {
			return B;
		}
		const float StepsAsFloat = static_cast<float>(Steps);
		const float NumIntervals = StepsAsFloat - 1.f;
		float const ModifiedAlpha = glm::floor(Alpha * StepsAsFloat) / NumIntervals;
		return glm::mix(A, B, ModifiedAlpha);
	}

	float InterpEaseIn(float A, float B, float Alpha, float Exp)
	{
		float const ModifiedAlpha = glm::pow(Alpha, Exp);
		return glm::mix(A, B, ModifiedAlpha);
	}

	float InterpEaseOut(float A, float B, float Alpha, float Exp)
	{
		float const ModifiedAlpha = 1.f - glm::pow(1.f - Alpha, Exp);
		return glm::mix(A, B, ModifiedAlpha);
	}

	float InterpEaseInOut(float A, float B, float Alpha, float Exp)
	{
		return glm::mix(A, B, (Alpha < 0.5f) ?
			InterpEaseIn(0.f, 1.f, Alpha * 2.f, Exp) * 0.5f :
			InterpEaseOut(0.f, 1.f, Alpha * 2.f - 1.f, Exp) * 0.5f + 0.5f);
	}

	float GetInterpolation(EZDTransitionInterpolation interpType, float amount, u32 stepCount, float easeExponent)
	{
		auto Result = amount;
		switch (interpType)
		{
		case EZDTransitionInterpolation::Jump:
			Result = std::floorf(amount);
			break;
		case EZDTransitionInterpolation::Linear:
			Result = amount;
			break;
		case EZDTransitionInterpolation::Step:
			Result = InterpStep(0.0f, 1.0f, amount, stepCount);
			break;
		case EZDTransitionInterpolation::EaseIn:
			Result = InterpEaseIn(0.0f, 1.0f, amount, easeExponent);
			break;
		case EZDTransitionInterpolation::EaseOut:
			Result = InterpEaseOut(0.0f, 1.0f, amount, easeExponent);
			break;
		case EZDTransitionInterpolation::EaseInOut:
			Result = InterpEaseInOut(0.0f, 1.0f, amount, easeExponent);
			break;
		}
		return Result;
	}
}