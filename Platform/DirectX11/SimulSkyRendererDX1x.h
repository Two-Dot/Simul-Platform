// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

// SimulSkyRendererDX1x.h A renderer for skies.

#pragma once
#include "Simul/Sky/SkyTexturesCallback.h"
#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Math/Matrix4x4.h"
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif

#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/HLSL/CppHLSL.hlsl"
#include "Simul/Platform/DirectX11/GpuSkyGenerator.h"

struct ID3DX11EffectShaderResourceVariable;
namespace simul
{
	namespace sky
	{
		class AtmosphericScatteringInterface;
		class Sky;
		class SkyKeyframer;
	}
}

typedef long HRESULT;

namespace simul
{
	namespace dx11
	{
		SIMUL_DIRECTX11_EXPORT_CLASS SimulSkyRendererDX1x:public simul::sky::BaseSkyRenderer
		{
		public:
			SimulSkyRendererDX1x(simul::sky::SkyKeyframer *sk,simul::base::MemoryInterface *mem);
			virtual ~SimulSkyRendererDX1x();
			//standard d3d object interface functions
			void RecompileShaders();
			//! Call this when the D3D device has been created or reset.
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			//! Call this when the D3D device has been shut down.
			void InvalidateDeviceObjects();
			//! Call this to release the memory for D3D device objects.
			bool Destroy();
			//! \deprecated This function is no longer used, as the sky is drawn by the atmospherics renderer. See simul::sky::BaseAtmosphericsRenderer.
			bool Render(void *context,bool blend);
			bool RenderPointStars(crossplatform::DeviceContext &deviceContext,float exposure);
			//! Draw the fade textures to screen
			bool RenderFades(crossplatform::DeviceContext &deviceContext,int x,int y,int w,int h);
			//! Call this to draw the sun flare, usually drawn last, on the main render target.
			bool RenderFlare(float exposure);
			bool Render2DFades(crossplatform::DeviceContext &deviceContext);
			void RenderIlluminationBuffer(crossplatform::DeviceContext &deviceContext);
			//! Get a value, from zero to one, which represents how much of the sun is visible.
			//! Call this when the current rendering surface is the one that has obscuring
			//! objects like mountains etc. in it, and make sure these have already been drawn.
			//! GetSunOcclusion executes a pseudo-render of an invisible billboard, then
			//! uses a hardware occlusion query to see how many pixels have passed the z-test.
			float CalcSunOcclusion(crossplatform::DeviceContext &deviceContext,float cloud_occlusion=0.f);
			//! Call this once per frame to set the matrices.
			void SetMatrices(const simul::math::Matrix4x4 &view,const simul::math::Matrix4x4 &proj);

			void Get2DLossAndInscatterTextures(void* *loss,void* *insc,void* *skyl,void* *overc);
			void *GetIlluminationTexture();
			void *GetLightTableTexture();

			float GetFadeInterp() const;
			void SetStepsPerDay(unsigned steps);
		//! Initialize textures
			void SetFadeTextureSize(unsigned width,unsigned height,unsigned num_altitudes);
			void FillFadeTex(ID3D11DeviceContext *context,int texture_index,int texel_index,int num_texels,
								const simul::sky::float4 *loss_float4_array,
								const simul::sky::float4 *inscatter_float4_array,
								const simul::sky::float4 *skylight_float4_array);
			void CycleTexturesForward();

			// for testing:
			void DrawCubemap(crossplatform::DeviceContext &deviceContext,ID3D11ShaderResourceView*	m_pCubeEnvMapSRV);
			simul::sky::BaseGpuSkyGenerator *GetBaseGpuSkyGenerator(){return &gpuSkyGenerator;}
		protected:
			int cycle;

			void CreateFadeTextures();
			void EnsureCorrectTextureSizes();
			void EnsureTexturesAreUpToDate(void *c);
			void EnsureTextureCycle();
	
			ID3D11Device*							m_pd3dDevice;
			ID3D11InputLayout*						m_pStarsVtxDecl;
			Query									sunQuery;

			ID3DX11EffectShaderResourceVariable*	flareTexture;
			ID3DX11EffectShaderResourceVariable*	inscTexture;
			ID3DX11EffectShaderResourceVariable*	skylTexture;
			ID3DX11EffectShaderResourceVariable*	fadeTexture1;
			ID3DX11EffectShaderResourceVariable*	fadeTexture2;
			ID3DX11EffectShaderResourceVariable*	illuminationTexture;

			// A framebuffer where x=azimuth, y=elevation, r=start depth, g=end depth.

			simul::dx11::GpuSkyGenerator		gpuSkyGenerator;
		};
	}
}