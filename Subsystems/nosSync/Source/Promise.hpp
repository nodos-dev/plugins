#pragma once

#include <Nodos/Plugin.hpp>

namespace nos::sync
{

nosResult NOSAPI_CALL CreatePromise(const char* tag, nosObjectReference* outPromise);
nosResult NOSAPI_CALL WaitPromise(nosObjectId promise, uint64_t timeoutNs);
nosResult NOSAPI_CALL FulfillPromise(nosObjectId promise);
nosResult NOSAPI_CALL ResetPromise(nosObjectId promise);

}