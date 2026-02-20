
#include "TextureTransitionUtils.h"

#include <glm/exponential.hpp>
#include <glm/common.hpp>


namespace nos::compositing
{
	float InterpStep(float A, float B, float Alpha, uint32_t Steps)
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

	float GetInterpolation(TransitionInterpolation interpType, float amount, uint32_t stepCount, float easeExponent)
	{
		auto Result = amount;
		switch (interpType)
		{
		case TransitionInterpolation::Jump:
			Result = std::floorf(amount);
			break;
		case TransitionInterpolation::Linear:
			Result = amount;
			break;
		case TransitionInterpolation::Step:
			Result = InterpStep(0.0f, 1.0f, amount, stepCount);
			break;
		case TransitionInterpolation::EaseIn:
			Result = InterpEaseIn(0.0f, 1.0f, amount, easeExponent);
			break;
		case TransitionInterpolation::EaseOut:
			Result = InterpEaseOut(0.0f, 1.0f, amount, easeExponent);
			break;
		case TransitionInterpolation::EaseInOut:
			Result = InterpEaseInOut(0.0f, 1.0f, amount, easeExponent);
			break;
		}
		return Result;
	}
}