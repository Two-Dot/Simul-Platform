#include "ThisPlatform/Direct3D12.h"
#include "Platform/CrossPlatform/Mesh.h"
#include "DirectXRaytracingHelper.h"
#include "Platform/DirectX12/SimulDirectXHeader.h"
#include "Platform/DirectX12/AccelerationStructure.h"
#include <algorithm>
#include <functional>

using namespace simul;
using namespace dx12;

static bool GetID3D12Device5andID3D12GraphicsCommandList4(crossplatform::DeviceContext& deviceContext, ID3D12Device5*& device5, ID3D12GraphicsCommandList4*& commandList4)
{
	ID3D12Device* device = deviceContext.renderPlatform->AsD3D12Device();
	device->QueryInterface(SIMUL_PPV_ARGS(&device5));
	if (!device5)
		return false;

	ID3D12GraphicsCommandList* commandList = deviceContext.asD3D12Context();
	commandList->QueryInterface(SIMUL_PPV_ARGS(&commandList4));
	if (!commandList4)
		return false;

	if (!deviceContext.renderPlatform->HasRenderingFeatures(crossplatform::RenderingFeatures::Raytracing))
		return false;

	return true;
}

////////////////////////////////////
//BottomLevelAccelerationStructure//
////////////////////////////////////

BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(crossplatform::RenderPlatform* r)
	:crossplatform::BottomLevelAccelerationStructure(r)
{
}

BottomLevelAccelerationStructure::~BottomLevelAccelerationStructure()
{
	InvalidateDeviceObjects();
}

void BottomLevelAccelerationStructure::RestoreDeviceObjects()
{
}

void BottomLevelAccelerationStructure::InvalidateDeviceObjects()
{
	initialized = false;
	SAFE_RELEASE(accelerationStructure);
	SAFE_RELEASE(scratchResource);
	SAFE_RELEASE(transforms);
}

ID3D12Resource* BottomLevelAccelerationStructure::AsD3D12ShaderResource(crossplatform::DeviceContext &deviceContext)
{
	if(!initialized)
		BuildAccelerationStructureAtRuntime(deviceContext);
	return accelerationStructure;
}

void BottomLevelAccelerationStructure::BuildAccelerationStructureAtRuntime(crossplatform::DeviceContext &deviceContext)
{
#if PLATFORM_SUPPORT_D3D12_RAYTRACING
	ID3D12Device* device = deviceContext.renderPlatform->AsD3D12Device();
	ID3D12Device5* device5 = nullptr;
	ID3D12GraphicsCommandList4* commandList4 = nullptr;
	bool platformValid = GetID3D12Device5andID3D12GraphicsCommandList4(deviceContext, device5, commandList4);

	if (!platformValid || !device5 || !commandList4)
	{
		// Initialized, but non-functional.
		initialized = true;
		return;
	}

	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;
	if (geometryType == crossplatform::BottomLevelAccelerationStructure::GeometryType::TRIANGLE_MESH)
	{
		crossplatform::Buffer* indexBuffer = mesh->GetIndexBuffer();
		crossplatform::Buffer* vertexBuffer = mesh->GetVertexBuffer();
		if (!indexBuffer | !vertexBuffer)
			return;
	
		auto* indexBuffer12 = indexBuffer->AsD3D12Buffer();
		auto* vertexBuffer12 = vertexBuffer->AsD3D12Buffer();
		unsigned indexSize = indexBuffer->stride;
		unsigned vertexSize = vertexBuffer->stride;
	
		// Build Transforms
		std::vector<mat4> transforms_cpu;
		std::function<void(crossplatform::Mesh::SubNode&, size_t&, math::Matrix4x4&)> FillTransformsFromNode = 
			[&](crossplatform::Mesh::SubNode& node, size_t& n, math::Matrix4x4& model)
		{
			const math::Matrix4x4& mat = node.orientation.GetMatrix();
			math::Matrix4x4 newModel;
			math::Multiply4x4(newModel, model, mat);
			model = newModel;
			mat4 m4;
			memcpy(&m4, newModel, sizeof(mat4));
			transforms_cpu.push_back(m4);
			for(int i = 0; i<node.children.size(); i++)
			{
				FillTransformsFromNode(node.children[i], n, model);
			}
			n++;
		};
		size_t nodeCount = 0;
		math::Matrix4x4 m = math::Matrix4x4::IdentityMatrix();
		FillTransformsFromNode(mesh->GetRootNode(), nodeCount, m);
		AllocateUploadBuffer(device, transforms_cpu.data(), transforms_cpu.size() * sizeof(mat4), &transforms, L"AccelerationStructure Transforms");
		
		// Build Geometry Descs
		std::function<void(crossplatform::Mesh::SubNode&, size_t&, size_t&)> FillGeometryFromNode =
			[&](crossplatform::Mesh::SubNode& node, size_t& n, size_t& g)
		{
			for(int i = 0; i < node.subMeshes.size(); i++)
			{
				crossplatform::Mesh::SubMesh *m=mesh->GetSubMesh(node.subMeshes[i]);
				if(!m)
					continue;
				D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc;
				geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
				geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
				// Mark the geometry as opaque. 
				// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
				// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
				geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
				geometryDesc.Triangles.IndexBuffer = indexBuffer12->GetGPUVirtualAddress() + (UINT64)m->IndexOffset*indexBuffer->stride;
				geometryDesc.Triangles.IndexCount = static_cast<UINT>(m->TriangleCount * 3) ;
				geometryDesc.Triangles.IndexFormat = indexBuffer->stride == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

				geometryDesc.Triangles.Transform3x4 = transforms->GetGPUVirtualAddress() + sizeof(mat4) * n;
				geometryDesc.Triangles.VertexCount = static_cast<UINT>(vertexBuffer->count);
				geometryDesc.Triangles.VertexBuffer.StartAddress =vertexBuffer12->GetGPUVirtualAddress();
				geometryDesc.Triangles.VertexBuffer.StrideInBytes = vertexSize;
				geometryDescs.push_back(geometryDesc);
				g++;
			}
			for (int i = 0; i < node.children.size(); i++)
			{
				FillGeometryFromNode(node.children[i], n, g);
			}
			n++;
		};
		size_t meshCount = 0;
		nodeCount = 0;
		FillGeometryFromNode(mesh->GetRootNode(), nodeCount, meshCount);
	}
	else if (geometryType == crossplatform::BottomLevelAccelerationStructure::GeometryType::AABB)
	{
		ID3D12Resource* ppAABBResources[1] = { nullptr };
		UINT64 AABBCount = _countof(ppAABBResources);
		ppAABBResources[0] = aabbBuffer->platformStructuredBuffer->AsD3D12Resource(deviceContext);

		//Will do the transition to D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE (from D3D12_RESOURCE_STATE_UNORDERED_ACCESS).
		auto srv = aabbBuffer->platformStructuredBuffer->AsD3D12ShaderResourceView(deviceContext);

		if (ppAABBResources == nullptr || AABBCount == 0)
			return;

		// Build Geometry Descs
		std::function<void()> FillGeometryFromNode = [&]()
		{
			for (int i = 0; i < AABBCount; i++)
			{
				D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc;
				geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
				geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
				geometryDesc.AABBs.AABBs.StartAddress = ppAABBResources[i]->GetGPUVirtualAddress() + (i * sizeof(D3D12_RAYTRACING_AABB));
				geometryDesc.AABBs.AABBs.StrideInBytes = sizeof(D3D12_RAYTRACING_AABB);
				geometryDesc.AABBs.AABBCount = 1;
				geometryDescs.push_back(geometryDesc);
			}
		};
		FillGeometryFromNode();
	}
	else if (geometryType == crossplatform::BottomLevelAccelerationStructure::GeometryType::UNKNOWN)
	{
		SIMUL_BREAK("ERROR: Provided geometry is BottomLevelAccelerationStructure::GeometryType::UNKNOWN");
	}
	else
	{
		SIMUL_BREAK("ERROR: BottomLevelAccelerationStructure::geometryType is invalid");
	}

	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	inputs.NumDescs = static_cast<UINT>(geometryDescs.size());
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.pGeometryDescs = geometryDescs.data();

	device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
	ThrowIfFalse(prebuildInfo.ResultDataMaxSizeInBytes > 0);

	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn’t need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
	AllocateUAVBuffer(device, prebuildInfo.ScratchDataSizeInBytes, &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"BottomLevelAccelerationStructure - ScratchResource");
	AllocateUAVBuffer(device, prebuildInfo.ResultDataMaxSizeInBytes, &accelerationStructure, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"BottomLevelAccelerationStructure - ResultResource");

	// Bottom Level Acceleration Structure desc
	buildDesc.DestAccelerationStructureData = accelerationStructure->GetGPUVirtualAddress();
	buildDesc.Inputs = inputs;
	buildDesc.SourceAccelerationStructureData = 0;
	buildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();

	commandList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
	commandList4->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(accelerationStructure));
#endif
	initialized=true;
}

/////////////////////////////////
//TopLevelAccelerationStructure//
/////////////////////////////////

TopLevelAccelerationStructure::TopLevelAccelerationStructure(crossplatform::RenderPlatform* r)
	:crossplatform::TopLevelAccelerationStructure(r)
{
}

TopLevelAccelerationStructure::~TopLevelAccelerationStructure()
{
	InvalidateDeviceObjects();
}

void TopLevelAccelerationStructure::RestoreDeviceObjects()
{
}

void TopLevelAccelerationStructure::InvalidateDeviceObjects()
{
	initialized = false;
	SAFE_RELEASE(accelerationStructure);
	SAFE_RELEASE(scratchResource);
	SAFE_RELEASE(instanceDescsResource);
}

ID3D12Resource* TopLevelAccelerationStructure::AsD3D12ShaderResource(crossplatform::DeviceContext& deviceContext)
{
	if (!initialized)
		BuildAccelerationStructureAtRuntime(deviceContext);
	return accelerationStructure;
}

void TopLevelAccelerationStructure::BuildAccelerationStructureAtRuntime(crossplatform::DeviceContext& deviceContext)
{
#if PLATFORM_SUPPORT_D3D12_RAYTRACING
	ID3D12Device* device = deviceContext.renderPlatform->AsD3D12Device();
	ID3D12Device5* device5 = nullptr;
	ID3D12GraphicsCommandList4* commandList4 = nullptr;
	bool platformValid = GetID3D12Device5andID3D12GraphicsCommandList4(deviceContext, device5, commandList4);

	if (!platformValid || !device5 || !commandList4)
	{
		// Initialized, but non-functional.
		initialized = true;
		return;
	}

	if (BLASandTransforms.empty())
		return;

	for (const auto& BLASandTransform : BLASandTransforms)
	{
		const auto& BLAS = BLASandTransform.first;
		const auto& Transform = BLASandTransform.second;
		BottomLevelAccelerationStructure* d3d12BLAS = (dx12::BottomLevelAccelerationStructure*)BLAS;

		// Create an instance desc for the bottom-level acceleration structure.
		D3D12_RAYTRACING_INSTANCE_DESC instanceDesc;
		instanceDesc.Transform[0][0] = Transform.m00;
		instanceDesc.Transform[0][1] = Transform.m01;
		instanceDesc.Transform[0][2] = Transform.m02;
		instanceDesc.Transform[0][3] = Transform.m03;
		instanceDesc.Transform[1][0] = Transform.m10;
		instanceDesc.Transform[1][1] = Transform.m11;
		instanceDesc.Transform[1][2] = Transform.m12;
		instanceDesc.Transform[1][3] = Transform.m13;
		instanceDesc.Transform[2][0] = Transform.m20;
		instanceDesc.Transform[2][1] = Transform.m21;
		instanceDesc.Transform[2][2] = Transform.m22;
		instanceDesc.Transform[2][3] = Transform.m23;
		instanceDesc.InstanceID = 0;
		instanceDesc.InstanceMask = 1;
		instanceDesc.InstanceContributionToHitGroupIndex = 0;
		instanceDesc.Flags = 0;
		instanceDesc.AccelerationStructure = d3d12BLAS->AsD3D12ShaderResource(deviceContext)->GetGPUVirtualAddress();
		instanceDescs.push_back(instanceDesc);
	}
	AllocateUploadBuffer(device, instanceDescs.data(), (instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC)), &instanceDescsResource, L"InstanceDescsResource");

	// Get required sizes for an acceleration structure.
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	inputs.NumDescs = static_cast<UINT>(BLASandTransforms.size());
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.InstanceDescs = instanceDescsResource->GetGPUVirtualAddress();

	device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
	ThrowIfFalse(prebuildInfo.ResultDataMaxSizeInBytes > 0);

	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn’t need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
	AllocateUAVBuffer(device, prebuildInfo.ScratchDataSizeInBytes, &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	AllocateUAVBuffer(device, prebuildInfo.ResultDataMaxSizeInBytes, &accelerationStructure, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"TopLevelAccelerationStructure");

	// Top Level Acceleration Structure desc
	buildDesc.DestAccelerationStructureData = accelerationStructure->GetGPUVirtualAddress();
	buildDesc.Inputs = inputs;
	buildDesc.SourceAccelerationStructureData = 0;
	buildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();

	commandList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
	commandList4->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(accelerationStructure));
#endif
	initialized = true;
}
