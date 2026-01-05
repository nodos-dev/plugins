#include <Nodos/Plugin.hpp>


namespace nos::utilities
{
NOS_REGISTER_NAME(PrintLog)
struct PrintLog : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(NodeExecuteParams const& params) override
	{
		const char* log = params.GetPinData<const char>(NOS_NAME("Log"));
		nos::log::LogLevel level = *params.GetPinData<nos::log::LogLevel>(NOS_NAME("LogLevel"));
		switch (level)
		{
		case nos::log::LogLevel::TRACE:
		case nos::log::LogLevel::DEBUG: nosEngine.LogD("%s", log); break;
		case nos::log::LogLevel::INFO: nosEngine.LogI("%s", log); break;
		case nos::log::LogLevel::WARNING: nosEngine.LogW("%s", log); break;
		case nos::log::LogLevel::ERROR_: nosEngine.LogE("%s", log); break;
		}
		return NOS_RESULT_SUCCESS;
	}

};


nosResult RegisterPrintLog(nosNodeFunctions* out)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("PrintLog"), PrintLog, out);
	return NOS_RESULT_SUCCESS;
}

}