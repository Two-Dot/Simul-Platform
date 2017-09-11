/*
	Heap.h 
*/

#pragma once

#include "Export.h"
#include "SimulDirectXHeader.h"

#include <string>

namespace simul
{
	namespace dx11on12
	{
		class RenderPlatform;
	}
	namespace dx12
	{
		/// DirectX12 Descriptor Heap helper class
		class SIMUL_DIRECTX12_EXPORT Heap
		{
		public:
			Heap();
			~Heap() {}
			/// Recreates the API DescriporHeap with the provided settings
			void Restore(dx11on12::RenderPlatform* renderPlatform, UINT totalCnt, D3D12_DESCRIPTOR_HEAP_TYPE type, const char* name = "Heap", bool shaderVisible = true);
			/// Offsets both CPU and GPU handles
			void Offset(int index = 1);
			CD3DX12_CPU_DESCRIPTOR_HANDLE CpuHandle();
			CD3DX12_GPU_DESCRIPTOR_HANDLE GpuHandle();
			UINT GetCount();
			ID3D12DescriptorHeap* GetHeap();
			/// Returns a CPU handle from the start of the heap at the provided offset
			D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandleFromStartAfOffset(UINT off);
			UINT GetHandleIncrement() { return mHandleIncrement; }
			/// This method won't release or destroy anything, it will reset the internal count of handles
			///	and make the held handles point at the start of the heap
			void Reset()
			{
				mCpuHandle = mHeap->GetCPUDescriptorHandleForHeapStart();
				mGpuHandle = mHeap->GetGPUDescriptorHandleForHeapStart();
				mCnt = 0;
			}
			void Release(dx11on12::RenderPlatform* r);

		private:
			ID3D12DescriptorHeap*			mHeap;
			CD3DX12_CPU_DESCRIPTOR_HANDLE	mCpuHandle;
			CD3DX12_GPU_DESCRIPTOR_HANDLE	mGpuHandle;
			UINT							mHandleIncrement;

			UINT							mTotalCnt;
			UINT							mCnt;
			std::string						mName;
		};
	}
}