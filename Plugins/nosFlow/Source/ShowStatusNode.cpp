// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>

namespace nos::flow
{
	NOS_REGISTER_NAME_SPACED(Nos_Utilities_ShowStatus, "nos.flow.ShowStatus")
	struct ShowStatus : NodeContext
	{
		nosResult ExecuteNode(NodeExecuteParams const& params) override
		{
			std::string statusMessage = "";
			const char* statusMsg = params.GetPinData<const char*>(NOS_NAME("Status"));
			if(strlen(statusMsg) > 0)
				statusMessage = std::string(statusMsg);
			fb::NodeStatusMessageType statusType = *params.GetPinData<fb::NodeStatusMessageType>(NOS_NAME("StatusType"));
			if (StatusMessage == statusMessage && StatusType == statusType)
			{
				return NOS_RESULT_SUCCESS;
			}
			StatusMessage = statusMessage;
			StatusType = statusType;
			if (statusMessage.empty())
				ClearNodeStatusMessages();
			else
				SetNodeStatusMessages({ {{}, std::move(statusMessage), statusType, "", 3, true, true}});
			return NOS_RESULT_SUCCESS;
		}

		std::string StatusMessage;
		fb::NodeStatusMessageType StatusType;
	};


	nosResult RegisterShowStatusNode(nosNodeFunctions* fn)
	{
		NOS_BIND_NODE_CLASS(NSN_Nos_Utilities_ShowStatus, ShowStatus, fn);
		return NOS_RESULT_SUCCESS;
	}

} // namespace nos::flow