#include "AccelerationStructure.h"
#include "ThisPlatform/Direct3D12.h"
#include "Platform/CrossPlatform/Mesh.h"
#include "Platform/DirectX12/SimulDirectXHeader.h"
#include "DirectXRaytracingHelper.h"
#include <algorithm>
#include <functional>

using namespace simul;
using namespace dx12;

AccelerationStructure::AccelerationStructure(crossplatform::RenderPlatform *r)
	:crossplatform::AccelerationStructure(r)
{
}

AccelerationStructure::~AccelerationStructure()
{
	InvalidateDeviceObjects();
}

void AccelerationStructure::RestoreDeviceObjects(crossplatform::Mesh *m)
{
	mesh=m;
}

ID3D12Resource* AccelerationStructure::AsD3D12ShaderResource(crossplatform::DeviceContext &deviceContext)
{
	if(!bottomLevelAccelerationStructure)
		RuntimeInit(deviceContext);
	return topLevelAccelerationStructure;
}

void AccelerationStructure::RuntimeInit(crossplatform::DeviceContext &deviceContext)
{
	ID3D12Device* device = renderPlatform->AsD3D12Device();
	ID3D12Device5 *device5=nullptr;
	device->QueryInterface(SIMUL_PPV_ARGS(&device5));
	if(!device5)
		return;
	auto commandList = deviceContext.asD3D12Context();
	ID3D12GraphicsCommandList4 *commandList4=nullptr;
	commandList->QueryInterface(SIMUL_PPV_ARGS(&commandList4));
	if(!commandList4)
		return;
	crossplatform::Buffer *indexBuffer=mesh->GetIndexBuffer();
	crossplatform::Buffer *vertexBuffer=mesh->GetVertexBuffer();
	if(!indexBuffer|!vertexBuffer)
		return;
	if(!renderPlatform->HasRenderingFeatures(crossplatform::RenderingFeatures::Raytracing))
	{
	// Initialized, but non-functional.
		initialized=true;
		return;
	}
	auto *indexBuffer12=indexBuffer->AsD3D12Buffer();
	auto *vertexBuffer12=vertexBuffer->AsD3D12Buffer();
	unsigned indexSize=indexBuffer->stride;
	unsigned vertexSize=vertexBuffer->stride;


	// Get required sizes for an acceleration structure.
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = buildFlags;
	topLevelInputs.NumDescs = 1;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	device5->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	ThrowIfFalse(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);
	
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs ;
	std::vector<mat4> transforms_cpu;
	std::function<void(crossplatform::Mesh::SubNode &node,size_t &n,math::Matrix4x4&)>  FillTransformsFromNode=[&] (crossplatform::Mesh::SubNode &node,size_t &n,math::Matrix4x4&model)
	{
		auto &mat= node.orientation.GetMatrix();
		math::Matrix4x4 newModel;
		math::Multiply4x4(newModel, model, mat);
		model=newModel;
		mat4 m4;
		memcpy(&m4,newModel,sizeof(mat4));
		transforms_cpu.push_back(m4);
		for(int i=0;i<node.children.size();i++)
		{
			FillTransformsFromNode(node.children[i],n,model);
		}
		n++;
	};
	size_t nodeCount=0;
	math::Matrix4x4 m=math::Matrix4x4::IdentityMatrix();
	FillTransformsFromNode(mesh->GetRootNode(),nodeCount,m);
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = topLevelInputs;
	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	
	AllocateUploadBuffer(device, transforms_cpu.data(), transforms_cpu.size()*sizeof(mat4), &transforms, L"AccelerationStructure Transforms");
	
	std::function<void(crossplatform::Mesh::SubNode &,size_t &,size_t &)>  FillGeometryFromNode=[&] (crossplatform::Mesh::SubNode &node,size_t &n,size_t &g)
	{
		for(int i=0;i<node.subMeshes.size();i++)
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
			geometryDesc.Triangles.IndexBuffer = indexBuffer12->GetGPUVirtualAddress()+m->IndexOffset*indexBuffer->stride;
			geometryDesc.Triangles.IndexCount = static_cast<UINT>(m->TriangleCount*3) ;
			geometryDesc.Triangles.IndexFormat = indexBuffer->stride==4?DXGI_FORMAT_R32_UINT:DXGI_FORMAT_R16_UINT;

			geometryDesc.Triangles.Transform3x4 = transforms->GetGPUVirtualAddress()+sizeof(mat4)*n;
			geometryDesc.Triangles.VertexCount = static_cast<UINT>(vertexBuffer->count);
			geometryDesc.Triangles.VertexBuffer.StartAddress =vertexBuffer12->GetGPUVirtualAddress();// m_vertexBuffer->GetGPUVirtualAddress();
			geometryDesc.Triangles.VertexBuffer.StrideInBytes = vertexSize;
			geometryDescs.push_back(geometryDesc);
			g++;
		}
		for(int i=0;i<node.children.size();i++)
		{
			FillGeometryFromNode(node.children[i],n,g);
		}
		n++;
	};
	size_t meshCount=0;
	nodeCount=0;
	FillGeometryFromNode(mesh->GetRootNode(),nodeCount,meshCount);
	bottomLevelInputs.pGeometryDescs = geometryDescs.data();
	bottomLevelInputs.NumDescs=meshCount;
	device5->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
	ThrowIfFalse(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	AllocateUAVBuffer(device,std::max(topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes), &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn’t need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
	{
		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	
		AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &bottomLevelAccelerationStructure, initialResourceState, L"BottomLevelAccelerationStructure");
		AllocateUAVBuffer(device, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &topLevelAccelerationStructure, initialResourceState, L"TopLevelAccelerationStructure");
	}

	// Create an instance desc for the bottom-level acceleration structure.
	D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
	instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
	instanceDesc.InstanceMask = 1;
	instanceDesc.AccelerationStructure = bottomLevelAccelerationStructure->GetGPUVirtualAddress();
	AllocateUploadBuffer(device, &instanceDesc, sizeof(instanceDesc), &instanceDescs, L"InstanceDescs");

	// Bottom Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	{
		bottomLevelBuildDesc.Inputs = bottomLevelInputs;
		bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = bottomLevelAccelerationStructure->GetGPUVirtualAddress();
	}

	// Top Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	{
		topLevelInputs.InstanceDescs = instanceDescs->GetGPUVirtualAddress();
		topLevelBuildDesc.Inputs = topLevelInputs;
		topLevelBuildDesc.DestAccelerationStructureData = topLevelAccelerationStructure->GetGPUVirtualAddress();
		topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
	}

	 commandList4->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
	 commandList4->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(bottomLevelAccelerationStructure));
	 commandList4->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
	 commandList4->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(topLevelAccelerationStructure));

	
	// Kick off acceleration structure construction.
	//m_deviceResources->ExecuteCommandList();

	// Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
	//m_deviceResources->WaitForGpu();
	initialized=true;
}

void AccelerationStructure::InvalidateDeviceObjects() 
{
	initialized=false;
	SAFE_RELEASE(bottomLevelAccelerationStructure);
	SAFE_RELEASE(topLevelAccelerationStructure);
	SAFE_RELEASE(scratchResource);
	SAFE_RELEASE(instanceDescs);
	SAFE_RELEASE(transforms);
}