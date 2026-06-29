// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include <Nodos/Plugin.hpp>
#include <glm/glm.hpp>

namespace nos::graphics
{

struct HomographySolverNodeContext : NodeContext
{
	using NodeContext::NodeContext;

	bool Failed = false;

	// Solve 8x8 linear system Ax = b using Gaussian elimination with partial pivoting
	static bool SolveLinearSystem(double A[8][8], double b[8], double x[8])
	{
		// Forward elimination with partial pivoting
		for (int col = 0; col < 8; col++)
		{
			// Find pivot
			int maxRow = col;
			double maxVal = std::abs(A[col][col]);
			for (int row = col + 1; row < 8; row++)
			{
				if (std::abs(A[row][col]) > maxVal)
				{
					maxVal = std::abs(A[row][col]);
					maxRow = row;
				}
			}

			if (maxVal < 1e-12)
				return false; // Singular matrix

			// Swap rows
			if (maxRow != col)
			{
				std::swap(b[col], b[maxRow]);
				for (int j = 0; j < 8; j++)
					std::swap(A[col][j], A[maxRow][j]);
			}

			// Eliminate below
			for (int row = col + 1; row < 8; row++)
			{
				double factor = A[row][col] / A[col][col];
				b[row] -= factor * b[col];
				for (int j = col; j < 8; j++)
					A[row][j] -= factor * A[col][j];
			}
		}

		// Back substitution
		for (int row = 7; row >= 0; row--)
		{
			x[row] = b[row];
			for (int j = row + 1; j < 8; j++)
				x[row] -= A[row][j] * x[j];
			x[row] /= A[row][row];
		}

		return true;
	}

	nosResult ExecuteNode(nos::NodeExecuteParams const& params) override
	{

		glm::vec2 src[4], dst[4];
		src[0] = *reinterpret_cast<const glm::vec2*>(params.GetPinValue<fb::vec2>(NOS_NAME("Source 1")));
		src[1] = *reinterpret_cast<const glm::vec2*>(params.GetPinValue<fb::vec2>(NOS_NAME("Source 2")));
		src[2] = *reinterpret_cast<const glm::vec2*>(params.GetPinValue<fb::vec2>(NOS_NAME("Source 3")));
		src[3] = *reinterpret_cast<const glm::vec2*>(params.GetPinValue<fb::vec2>(NOS_NAME("Source 4")));
		dst[0] = *reinterpret_cast<const glm::vec2*>(params.GetPinValue<fb::vec2>(NOS_NAME("Target 1")));
		dst[1] = *reinterpret_cast<const glm::vec2*>(params.GetPinValue<fb::vec2>(NOS_NAME("Target 2")));
		dst[2] = *reinterpret_cast<const glm::vec2*>(params.GetPinValue<fb::vec2>(NOS_NAME("Target 3")));
		dst[3] = *reinterpret_cast<const glm::vec2*>(params.GetPinValue<fb::vec2>(NOS_NAME("Target 4")));

		// DLT: For each correspondence (x,y) -> (x',y') with h33 = 1:
		//   h11*x + h12*y + h13 - h31*x*x' - h32*y*x' = x'
		//   h21*x + h22*y + h23 - h31*x*y' - h32*y*y' = y'
		// Unknowns: h11, h12, h13, h21, h22, h23, h31, h32

		double A[8][8] = {};
		double b[8] = {};

		for (int i = 0; i < 4; i++)
		{
			double sx = src[i].x, sy = src[i].y;
			double dx = dst[i].x, dy = dst[i].y;

			int r0 = i * 2;
			int r1 = i * 2 + 1;

			// Row for x': h11*sx + h12*sy + h13 - h31*sx*dx - h32*sy*dx = dx
			A[r0][0] = sx;  A[r0][1] = sy;  A[r0][2] = 1.0;
			A[r0][3] = 0.0; A[r0][4] = 0.0; A[r0][5] = 0.0;
			A[r0][6] = -sx * dx; A[r0][7] = -sy * dx;
			b[r0] = dx;

			// Row for y': h21*sx + h22*sy + h23 - h31*sx*dy - h32*sy*dy = dy
			A[r1][0] = 0.0; A[r1][1] = 0.0; A[r1][2] = 0.0;
			A[r1][3] = sx;  A[r1][4] = sy;  A[r1][5] = 1.0;
			A[r1][6] = -sx * dy; A[r1][7] = -sy * dy;
			b[r1] = dy;
		}

		double h[8] = {};
		if (!SolveLinearSystem(A, b, h))
		{
			if (!Failed)
			{
				Failed = true;
				SetNodeStatusMessage("Degenerate point configuration", fb::NodeStatusMessageType::FAILURE);
			}
			return NOS_RESULT_FAILED;
		}

		if (Failed)
		{
			Failed = false;
			ClearNodeStatusMessages();
		}

		// Construct 3x3 homography matrix (column-major for GLM)
		glm::mat3 H(
			(float)h[0], (float)h[3], (float)h[6],  // column 0
			(float)h[1], (float)h[4], (float)h[7],  // column 1
			(float)h[2], (float)h[5], 1.0f           // column 2
		);

		SetPinValue(NOS_NAME("Homography"), reinterpret_cast<fb::mat3&>(H));

		return NOS_RESULT_SUCCESS;
	}
};

nosResult RegisterHomographySolver(nosNodeFunctions* fn)
{
	NOS_BIND_NODE_CLASS(NOS_NAME("nos.graphics.HomographySolver"), HomographySolverNodeContext, fn)
	return NOS_RESULT_SUCCESS;
}

} // namespace nos::graphics
