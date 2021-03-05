#pragma once
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Shaders/SL/raytracing_constants.sl"
#include "Platform/CrossPlatform/PlatformStructuredBuffer.h"
#include "BaseAccelerationStructure.h"
namespace simul
{
	namespace crossplatform
	{
		// Derived class for Bottom Level Acceleration Structures, which contains geometry to be built.
		class SIMUL_CROSSPLATFORM_EXPORT BottomLevelAccelerationStructure : public BaseAccelerationStructure
		{
		public:
			enum class GeometryType : uint32_t
			{
				UNKNOWN,
				TRIANGLE_MESH,
				AABB
			};

		protected:
			crossplatform::Mesh* mesh = nullptr;
			crossplatform::Raytracing_AABB_SB* aabbBuffer = nullptr;
			GeometryType geometryType = GeometryType::UNKNOWN;

		public:
			BottomLevelAccelerationStructure(crossplatform::RenderPlatform* r);
			virtual ~BottomLevelAccelerationStructure();

			void SetMesh(crossplatform::Mesh* mesh);
			void SetAABB(crossplatform::StructuredBuffer<Raytracing_AABB>* aabbBuffer);

			virtual void BuildAccelerationStructureAtRuntime(DeviceContext& deviceContext) { initialized = true; }
		};
	}
}