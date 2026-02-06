// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/PluginHelpers.hpp>
#include <Graphics_generated.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace nos::graphics
{
NOS_REGISTER_NAME(BillboardPositions)

NOS_REGISTER_NAME(RenderView)
NOS_REGISTER_NAME(Position)
NOS_REGISTER_NAME(Width)
NOS_REGISTER_NAME(Height)
NOS_REGISTER_NAME(BottomLeft)
NOS_REGISTER_NAME(BottomRight)
NOS_REGISTER_NAME(TopLeft)
NOS_REGISTER_NAME(TopRight)

struct BillboardPositions : NodeContext
{
	using NodeContext::NodeContext;

	nosResult ExecuteNode(nosNodeExecuteParams* params) override
	{
		NodeExecuteParams pins(params);
		TRenderView view = pins.GetPinData<TRenderView>(NSN_RenderView);
		float width = *pins.GetPinData<float>(NSN_Width);
		float height = *pins.GetPinData<float>(NSN_Height);
		glm::vec3 position = *pins.GetPinData<glm::vec3>(NSN_Position);

		glm::mat4 viewMat = reinterpret_cast<glm::mat4&>(view.view);

		// Face the camera (billboard orientation)
		glm::vec3 cameraPos = glm::vec3(glm::inverse(viewMat)[3]);
		cameraPos.z = position.z;
		glm::vec3 toCamera = glm::normalize(cameraPos - position);
		glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
		glm::vec3 right = glm::normalize(glm::cross(toCamera, up));

		// Calculate corner positions
		// Height starts from the bottom (position is at the bottom center)
		float halfWidth = width * 0.5f;

		glm::vec3 bottomLeft = position - right * halfWidth;
		glm::vec3 bottomRight = position + right * halfWidth;
		glm::vec3 topLeft = position - right * halfWidth + up * height;
		glm::vec3 topRight = position + right * halfWidth + up * height;

		// Set output pins
		SetPinValue(NSN_BottomLeft, nos::Buffer::From(bottomLeft));
		SetPinValue(NSN_BottomRight, nos::Buffer::From(bottomRight));
		SetPinValue(NSN_TopLeft, nos::Buffer::From(topLeft));
		SetPinValue(NSN_TopRight, nos::Buffer::From(topRight));

		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterBillboardPositions(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NSN_BillboardPositions, BillboardPositions, fn);
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::graphics
