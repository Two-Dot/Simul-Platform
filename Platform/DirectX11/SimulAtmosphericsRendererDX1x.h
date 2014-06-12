// Copyright (c) 2007-2009 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include <tchar.h>
#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif

#include "Simul/Math/Matrix4x4.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Sky/BaseAtmosphericsRenderer.h"
typedef long HRESULT;
struct ID3DX11EffectShaderResourceVariable;
namespace simul
{
	namespace sky
	{
		class AtmosphericScatteringInterface;
	}
}

namespace simul
{
	namespace dx11
	{
		// A class that takes an image buffer and a z-buffer and applies atmospheric fading.
		class SimulAtmosphericsRendererDX1x : public simul::sky::BaseAtmosphericsRenderer
		{
		public:
			SimulAtmosphericsRendererDX1x(simul::base::MemoryInterface *m);
			virtual ~SimulAtmosphericsRendererDX1x();
			
			void SetBufferSize(int w,int h);

			// BaseAtmosphericsRenderer.
			void SetLossTexture(void* t);
			void SetInscatterTextures(void* i,void *s,void *o);
			void SetIlluminationTexture(void *i);
			void SetCloudsTexture(void* t);
			void RecompileShaders();

			//! Call when we've got a fresh d3d device - on startup or when the device has been restored.
			void RestoreDeviceObjects(void* pd3dDevice);
			//! Call this when the device has been lost.
			void InvalidateDeviceObjects();
			void SetMatrices(const simul::math::Matrix4x4 &view,const simul::math::Matrix4x4 &proj);
			//! Render the Atmospherics.
			void RenderAsOverlay(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *hiResDepthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH);
			virtual void RenderLoss(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,const simul::sky::float4& relativeViewportTextureRegionXYWH,bool near_pass);
			virtual void RenderInscatter(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH,bool near_pass);
			void RenderGodrays(crossplatform::DeviceContext &deviceContext,float strength,bool near_pass,crossplatform::Texture *depthTexture,float exposure,const simul::sky::float4& relativeViewportTextureRegionXYWH,crossplatform::Texture *cloudDepthTexture);
		protected:
			HRESULT Destroy();
			ID3D11Device*								m_pd3dDevice;
			simul::math::Matrix4x4						view,proj;

			//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
			ID3DX11Effect*								effect;

			ID3DX11EffectTechnique*						godraysTechnique;
			// Variables for this effect:
			ID3DX11EffectShaderResourceVariable*			depthTexture;
			ID3DX11EffectShaderResourceVariable*			cloudDepthTexture;
			ID3DX11EffectShaderResourceVariable*			lossTexture;
			ID3DX11EffectShaderResourceVariable*			inscTexture;
			ID3DX11EffectShaderResourceVariable*			skylTexture;
			ID3DX11EffectShaderResourceVariable*			illuminationTexture;
			ID3DX11EffectShaderResourceVariable*			overcTexture;
			ID3DX11EffectShaderResourceVariable*			cloudShadowTexture;
			ID3DX11EffectShaderResourceVariable*				cloudGodraysTexture;

			ID3D11ShaderResourceView*					skyLossTexture_SRV;
			ID3D11ShaderResourceView*					skyInscatterTexture_SRV;
			ID3D11ShaderResourceView*					overcInscTexture_SRV;
			ID3D11ShaderResourceView*					skylightTexture_SRV;

			ID3D11ShaderResourceView*					illuminationTexture_SRV;
			ID3D11ShaderResourceView*				rainbowLookupTexture;
			ID3D11ShaderResourceView*				coronaLookupTexture;

			ConstantBuffer<AtmosphericsPerViewConstants>	atmosphericsPerViewConstants;
			ConstantBuffer<AtmosphericsUniforms>			atmosphericsUniforms;
		};
	}
}