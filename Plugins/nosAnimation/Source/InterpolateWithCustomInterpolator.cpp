#include <Nodos/Plugin.hpp>
#include <nosAnimationSubsystem/nosAnimationSubsystem.h>

namespace nos::animation
{
nosResult RegisterInterpolateWithCustomInterpolator(nosNodeFunctions* out)
{
	out->ClassName = NOS_NAME("InterpolateWithCustomInterpolator");
	out->ExecuteNode = [](void*, nosNodeExecuteParams* params) -> nosResult {
		NodeExecuteParams args(params);
		nos::Name typeName = args[NOS_NAME("A")].TypeName;
		if (typeName == NOS_NAME(Generic::GetFullyQualifiedName()))
		{
			nosEngine.LogW("InterpolateWithCustomInterpolator: Type is not resolved");
			return NOS_RESULT_FAILED;
		}
		nosBuffer a = *args[NOS_NAME("A")].Data;
		nosBuffer b = *args[NOS_NAME("B")].Data;
		double t = *args.GetPinData<double>(NOS_NAME("t"));
		EngineBuffer outBuf{};
		nosResult res = nosAnimation->Interpolate(typeName, a, b, t, outBuf.GetBufferPtr());
		if (res != NOS_RESULT_SUCCESS)
		{
			nosEngine.LogW("InterpolateWithCustomInterpolator: Interpolation failed");
			return res;
		}
		nosEngine.SetPinValue(args[NOS_NAME("Out")].Id, outBuf);
		return NOS_RESULT_SUCCESS;
	};
	return NOS_RESULT_SUCCESS;
}
} // namespace nos::animation