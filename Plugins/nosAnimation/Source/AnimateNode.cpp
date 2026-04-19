#include <Nodos/Plugin.hpp>


namespace nos::animation
{
NOS_REGISTER_NAME(t)
NOS_REGISTER_NAME(AnimationFrame)
struct AnimateNode : NodeContext
{
	bool StartNextFrame = false;
	bool Running = false;
	bool TransientReverse = false;

	std::chrono::nanoseconds VariableStepTotalRuntime;

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		if (StartNextFrame)
		{
			StartNextFrame = false;
			TransientReverse = false;
			Running = true;
			ResetAnimationFrameNum();
			nosEngine.TriggerNodeEvent(NodeId, NOS_NAME("Started"));
		}
		if (Running)
		{
			bool reverse = *params.GetPinValue<bool>(NOS_NAME("Reverse")) || TransientReverse;
			double animationDuration = *params.GetPinValue<double>(NOS_NAME("Duration"));

			double out = 0.0;
			if (params.GetVariableStepTiming())
				out = std::chrono::duration<double>(VariableStepTotalRuntime).count();
			else
				out = params.GetTotalTime(*params.GetPinValue<uint64_t>(NSN_AnimationFrame));
			out /= animationDuration;
			bool loop = *params.GetPinValue<bool>(NOS_NAME("Loop"));
			bool finished = false;
			if (loop)
			{
				bool swing = *params.GetPinValue<bool>(NOS_NAME("Swing"));
				int64_t loops = std::floor(out);
				bool isEven = loops % 2 == 0;
				if (swing && !isEven)
					out = loops + 1 - out;
				else
					out = out - loops;
			}
			else
			{
				if (!reverse && out >= 1.0f)
				{
					out = 1.0f;
					finished = true;
				}
				if (reverse && out <= 0.0f)
				{
					TransientReverse = false;
					ResetAnimationFrameNum();
					out = 0.0f;
					finished = true;
				}
			}
			SetPinValue(NSN_t, nos::Buffer::From(out));
			if (finished)
			{
				nosEngine.TriggerNodeEvent(NodeId, NOS_NAME("Finished"));
				Running = false;
				TransientReverse = false;
			}
			else
			{
				if (auto variableStep = params.GetVariableStepTiming())
					VariableStepTotalRuntime +=
						std::chrono::nanoseconds(variableStep->TimeSinceLastFrameNs) * (reverse ? -1 : 1);
				else
					SetPinValue(NSN_AnimationFrame, nos::Buffer::From(*params.GetPinValue<uint64_t>(NSN_AnimationFrame) + (reverse ? -1 : 1)));

			}
			return NOS_RESULT_SUCCESS;
		}
		params.MarkAllOutsDirty = false;
		return NOS_RESULT_SUCCESS;
	}

	nosResult Start(nosFunctionExecuteParams* functionExecParams)
	{
		StartNextFrame = true;
		return NOS_RESULT_SUCCESS;
	}

	nosResult Pause(nosFunctionExecuteParams* functionExecParams)
	{
		TransientReverse = false;
		Running = false;
		return NOS_RESULT_SUCCESS;
	}

	nosResult Continue(nosFunctionExecuteParams* functionExecParams)
	{
		Running = true;
		return NOS_RESULT_SUCCESS;
	}

	void ResetAnimationFrameNum()
	{
		SetPinValue(NSN_AnimationFrame, nos::Buffer::From(0ull));
		VariableStepTotalRuntime = std::chrono::nanoseconds{};
	}

	nosResult Reset(nosFunctionExecuteParams* functionExecParams)
	{
		ResetAnimationFrameNum();
		Running = false;
		double out = 0.0f;
		SetPinValue(NSN_t, nos::Buffer::From(out));
		return NOS_RESULT_SUCCESS;
	}

	nosResult ForwardContinue(nosFunctionExecuteParams* functionExecParams)
	{
		SetPinValue(NOS_NAME("Reverse"), nos::Buffer::From(false));
		TransientReverse = false;
		Running = true;
		return NOS_RESULT_SUCCESS;
	}

	nosResult ReversedContinue(nosFunctionExecuteParams* functionExecParams)
	{
		TransientReverse = true;
		Running = true;
		return NOS_RESULT_SUCCESS;
	}

	NOS_DECLARE_FUNCTIONS_FOR(AnimateNode,
		NOS_ADD_FUNCTION(NOS_NAME("Start"), Start),
		NOS_ADD_FUNCTION(NOS_NAME("Pause"), Pause),
		NOS_ADD_FUNCTION(NOS_NAME("Continue"), Continue),
		NOS_ADD_FUNCTION(NOS_NAME("Reset"), Reset),
		NOS_ADD_FUNCTION(NOS_NAME("ForwardContinue"), ForwardContinue),
		NOS_ADD_FUNCTION(NOS_NAME("ReversedContinue"), ReversedContinue),
	);
};


nosResult RegisterAnimate(nosNodeFunctions* out)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("Animate"), AnimateNode, out);
	return NOS_RESULT_SUCCESS;
}

}