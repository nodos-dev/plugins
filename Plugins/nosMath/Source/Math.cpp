// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginAPI.h>

#include <Builtins_generated.h>
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <chrono>
#include <Nodos/Plugin.hpp>

NOS_INIT()

NOS_BEGIN_IMPORT_DEPS()
NOS_END_IMPORT_DEPS()

NOS_REGISTER_NAME(X);
NOS_REGISTER_NAME(Y);
NOS_REGISTER_NAME(Z);
NOS_REGISTER_NAME(Position);
NOS_REGISTER_NAME(Rotation);
NOS_REGISTER_NAME(Transformation);
NOS_REGISTER_NAME(FOV);

// TODO: Transfer do not allow changing outputs

namespace nos::math
{

#define NO_ARG

#define DEF_OP0(o, n, t) nos::fb::vec##n##t operator o(nos::fb::vec##n##t l, nos::fb::vec##n##t r) { (glm::t##vec##n&)l += (glm::t##vec##n&)r; return (nos::fb::vec##n##t&)l; }
#define DEF_OP1(n, t) DEF_OP0(+, n, t) DEF_OP0(-, n, t) DEF_OP0(*, n, t) DEF_OP0(/, n, t)
#define DEF_OP(t) DEF_OP1(2, t) DEF_OP1(3, t) DEF_OP1(4, t)

DEF_OP(u);
DEF_OP(i);
DEF_OP(d);
DEF_OP(NO_ARG);

template<class T> T Add(T x, T y) { return x + y; }
template<class T> T Sub(T x, T y) { return x - y; }
template<class T> T Mul(T x, T y) { return x * y; }
template<class T> T Div(T x, T y) { return x / y; }

template<class T, int N>
struct Vec {
	T C[N] = {};

	Vec() = default;

	template<class P>
	Vec(const P* p) : C{}
	{
		C[0] = p->x();
		C[1] = p->y();
		if constexpr (N > 2) C[2] = p->z();
		if constexpr (N > 3) C[3] = p->w();
	}

	template<T F(T, T)>
	Vec Binop(Vec r) const
	{
		Vec<T, N> result = {};
		for (int i = 0; i < N; i++)
			result.C[i] = F(C[i], r.C[i]);
		return result;
	}

	Vec operator +(Vec r) const { return Binop<Add>(r); }
	Vec operator -(Vec r) const { return Binop<Sub>(r); }
	Vec operator *(Vec r) const { return Binop<Mul>(r); }
	Vec operator /(Vec r) const { return Binop<Div>(r); }
};

enum class MathNodeTypes : int {
	Clamp,
	Absolute,
	AddTransform,
	PerspectiveView,
	Eval,
	Transform,
	ToTransformMatrix,
	Inverse,
	Transpose,
	And,
	Or,
	Not,
	Random,
	SineWave,
	EulerToQuaternion,
	QuaternionToEuler,
	QuaternionMultiply,
	Count
};

template<u32 hi, class F, u32 i = 0>
void For(F&& f)
{
	if constexpr (i < hi)
	{
		f.template operator() < i > ();
		For<hi, F, i + 1>(std::move(f));
	}
}

template<class T, class F>
void FieldIterator(F&& f)	
{
	For<T::Traits::fields_number>([f = std::move(f), ref = T::MiniReflectTypeTable()]<u32 i>() {
		using Type = std::remove_pointer_t<typename T::Traits::template FieldType<i>>;
		f.template operator() < i, Type > (ref->values ? ref->values[i] : 0);
	});
}

nosResult AddTransform(void* ctx, nosNodeExecuteParams* params)
{
	NodeExecuteParams execParams(params);
	nos::Buffer outBuf = execParams.GetPinBuffer(NSN_Z);
	FieldIterator<fb::Transform>([X = execParams.GetPinValue<uint8_t>(NSN_X),
								  Y = execParams.GetPinValue<uint8_t>(NSN_Y),
								  Z = outBuf.As<uint8_t>()]<u32 i, class T>(auto O) {
		if constexpr (i == 2) (T&)O[Z] = (T&)O[X] * (T&)O[Y];
		else (T&)O[Z] = (T&)O[X] + (T&)O[Y];
	});
	SetPinValue(execParams[NSN_Z].Id, outBuf);
	return NOS_RESULT_SUCCESS;
}

void RegisterEval(nosNodeFunctions*);
void RegisterTransform(nosNodeFunctions*);
void RegisterToTransformMatrix(nosNodeFunctions*);
void RegisterInverse(nosNodeFunctions*);
void RegisterTranspose(nosNodeFunctions*);
void RegisterAnd(nosNodeFunctions*);
void RegisterOr(nosNodeFunctions*);
void RegisterNot(nosNodeFunctions*);
void RegisterRandom(nosNodeFunctions*);
void RegisterSineWave(nosNodeFunctions*);
void RegisterEulerToQuaternion(nosNodeFunctions*);
void RegisterQuaternionToEuler(nosNodeFunctions*);
void RegisterQuaternionMultiply(nosNodeFunctions*);

nosResult NOSAPI_CALL ExportNodeFunctions(size_t* outCount, nosNodeFunctions** outList)
{
	*outCount = (size_t)(MathNodeTypes::Count);
	if (!outList)
		return NOS_RESULT_SUCCESS;
	for (int i = 0; i < int(MathNodeTypes::Count); ++i)
	{
		auto node = outList[i];
		switch ((MathNodeTypes)i)
		{
		case MathNodeTypes::SineWave: {
			RegisterSineWave(node);
			break;
		}
		case MathNodeTypes::Clamp: {
			node->ClassName = NOS_NAME_STATIC("nos.math.Clamp");
			node->ExecuteNode = [](void* ctx, nosNodeExecuteParams* params) {
				constexpr uint32_t PIN_IN = 0;
				constexpr uint32_t PIN_MIN = 1;
				constexpr uint32_t PIN_MAX = 2;
				constexpr uint32_t PIN_OUT = 3;
				auto value = *InterpretObject<float>(*params->Pins[PIN_IN]->Object);
				auto min = *InterpretObject<float>(*params->Pins[PIN_MIN]->Object);
				auto max = *InterpretObject<float>(*params->Pins[PIN_MAX]->Object);
				SetPinValue(params->Pins[PIN_OUT]->Id, std::clamp(value, min, max));
				return NOS_RESULT_SUCCESS;
			};
			break;
		}
		case MathNodeTypes::Absolute: {
			node->ClassName = NOS_NAME_STATIC("nos.math.Absolute");
			node->ExecuteNode = [](void* ctx, nosNodeExecuteParams* params) {
				constexpr uint32_t PIN_IN = 0;
				constexpr uint32_t PIN_OUT = 1;
				auto value = *InterpretObject<float>(*params->Pins[PIN_IN]->Object);
				SetPinValue(params->Pins[PIN_OUT]->Id, std::abs(value));
				return NOS_RESULT_SUCCESS;
				};
			break;
		}
		case MathNodeTypes::AddTransform: {
			node->ClassName = NOS_NAME_STATIC("nos.math.Add_Transform");
			node->ExecuteNode = AddTransform;
			break;
		}
		case MathNodeTypes::PerspectiveView: {
			node->ClassName = NOS_NAME_STATIC("nos.math.PerspectiveView");
			node->ExecuteNode = [](void* ctx, nosNodeExecuteParams* params)
				{
				NodeExecuteParams execParams(params);
					auto fov = *execParams.GetPinValue<float>(NSN_FOV);

					// Sanity checks
					static_assert(alignof(glm::vec3) == alignof(nos::fb::vec3));
					static_assert(sizeof(glm::vec3) == sizeof(nos::fb::vec3));
					static_assert(alignof(glm::mat4) == alignof(nos::fb::mat4));
					static_assert(sizeof(glm::mat4) == sizeof(nos::fb::mat4));

					// glm::dvec3 is compatible with nos::fb::vec3d so it's safe to cast
					auto const& rot = *execParams.GetPinValue<glm::vec3>(NSN_Rotation);
					auto const& pos = *execParams.GetPinValue<glm::vec3>(NSN_Position);
					auto perspective = glm::perspective(fov, 16.f / 9.f, 10.f, 10000.f);
					auto view = glm::eulerAngleXYZ(rot.x, rot.y, rot.z);
					SetPinValue(execParams[NSN_Transformation].Id, perspective * view);
					return NOS_RESULT_SUCCESS;
				};
			break;
		}
		case MathNodeTypes::Eval: {
			RegisterEval(node);
			break;
		}
		case MathNodeTypes::Transform: {
			RegisterTransform(node);
			break;
		}
		case MathNodeTypes::ToTransformMatrix: {
			RegisterToTransformMatrix(node);
			break;
		}
		case MathNodeTypes::Inverse: {
			RegisterInverse(node);
			break;
		}
		case MathNodeTypes::Transpose: {
			RegisterTranspose(node);
			break;
		}
		case MathNodeTypes::And: {
			RegisterAnd(node);
			break;
		}
		case MathNodeTypes::Or: {
			RegisterOr(node);
			break;
		}
		case MathNodeTypes::Not: {
			RegisterNot(node);
			break;
		}
		case MathNodeTypes::Random: {
			RegisterRandom(node);
			break;
		}
		case MathNodeTypes::EulerToQuaternion: {
			RegisterEulerToQuaternion(node);
			break;
		}
		case MathNodeTypes::QuaternionToEuler: {
			RegisterQuaternionToEuler(node);
			break;
		}
		case MathNodeTypes::QuaternionMultiply: {
			RegisterQuaternionMultiply(node);
			break;
		}
		default:
			break;
		}
	}

	return NOS_RESULT_SUCCESS;
}

extern "C"
{
NOSAPI_ATTR nosResult NOSAPI_CALL nosExportPlugin(nosPluginFunctions* outFunctions)
{
	outFunctions->ExportNodeFunctions = ExportNodeFunctions;
	return NOS_RESULT_SUCCESS;
}
} // extern "C"

} // namespace nos::math
