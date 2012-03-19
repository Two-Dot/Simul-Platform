// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// SimulWeatherRendererDX1x.cpp A renderer for skies, clouds and weather effects.

#include "SimulWeatherRendererDX1x.h"

#include <dxerr.h>
#include <string>

#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Base/StringToWString.h"
#include "SimulSkyRendererDX1x.h"
#include "SimulAtmosphericsRendererDX1x.h"

#include "SimulCloudRendererDX1x.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include "MacrosDX1x.h"

D3DXMATRIX view_matrices[6];

SimulWeatherRendererDX1x::SimulWeatherRendererDX1x(simul::clouds::Environment *env,
		bool usebuffer,bool tonemap,int w,int h,bool sky,bool clouds3d,bool clouds2d,bool rain) :
	BaseWeatherRenderer(env),
	framebuffer(w,h),
	m_pd3dDevice(NULL),
	m_pImmediateContext(NULL),

	simulSkyRenderer(NULL),
	simulCloudRenderer(NULL),
	show_3d_clouds(clouds3d),
	layer2(clouds2d),
	BufferWidth(w),
	BufferHeight(h),
	timing(0.f),
	exposure_multiplier(1.f),
	show_sky(sky),
	show_rain(rain)
{
	simul::sky::SkyKeyframer *sk=env->skyKeyframer.get();
	simul::clouds::CloudKeyframer *ck2d=env->cloud2DKeyframer.get();
	simul::clouds::CloudKeyframer *ck3d=env->cloudKeyframer.get();
	if(show_sky)
	{
		simulSkyRenderer=new SimulSkyRendererDX1x(sk);
		baseSkyRenderer=simulSkyRenderer.get();
		Group::AddChild(simulSkyRenderer.get());
	}
	if(show_3d_clouds)
	{
		simulCloudRenderer=new SimulCloudRendererDX1x(ck3d);
		baseCloudRenderer=simulCloudRenderer.get();
		Group::AddChild(simulCloudRenderer.get());
	}
/*	if(clouds2d)
		simul2DCloudRenderer=new Simul2DCloudRenderer(ck2d);
	if(rain)
		simulPrecipitationRenderer=new SimulPrecipitationRenderer();
	*/
	simulAtmosphericsRenderer=new SimulAtmosphericsRendererDX1x;
	baseAtmosphericsRenderer=simulAtmosphericsRenderer.get();
	ConnectInterfaces();
}
void SimulWeatherRendererDX1x::ConnectInterfaces()
{
	if(simulCloudRenderer.get()&&simulSkyRenderer.get())
	{
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
		simulSkyRenderer->SetOvercastCallback(simulCloudRenderer->GetOvercastCallback());
	}
/*	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
	}*/
	if(simulSkyRenderer)
	{
	//	if(simulCloudRenderer)
//			simulSkyRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
	}
	//if(simulAtmosphericsRenderer&&simulSkyRenderer)
	//	simulAtmosphericsRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
	/*if(simul2DCloudRenderer)
		V_RETURN(simul2DCloudRenderer->RestoreDeviceObjects(m_pd3dDevice));
	if(simulPrecipitationRenderer)
		V_RETURN(simulPrecipitationRenderer->RestoreDeviceObjects(m_pd3dDevice));
	*/
	//if(simulAtmosphericsRenderer)
	//	simulAtmosphericsRenderer->RestoreDeviceObjects(m_pd3dDevice);
}

bool SimulWeatherRendererDX1x::RestoreDeviceObjects(ID3D1xDevice* dev,IDXGISwapChain *swapChain)
{
	HRESULT hr=S_OK;
	m_pd3dDevice=dev;
	
#ifdef DX10
	m_pImmediateContext=dev;
#else
	SAFE_RELEASE(m_pImmediateContext);
	m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
#endif

	framebuffer.RestoreDeviceObjects(m_pd3dDevice);
	framebuffer_cubemap.SetWidthAndHeight(64,64);
	framebuffer_cubemap.RestoreDeviceObjects(m_pd3dDevice);

	pSwapChain=swapChain;
// Get the back buffer (screen) format so we know how to render the weather buffer to the screen:
	ID3D1xTexture2D *pBackBuffer=NULL;
	pSwapChain->GetBuffer(0,__uuidof(ID3D1xTexture2D),(void**)&pBackBuffer);
	D3D1x_TEXTURE2D_DESC desc;
	pBackBuffer->GetDesc(&desc);
	SAFE_RELEASE(pBackBuffer);
	ScreenWidth=desc.Width;
	ScreenHeight=desc.Height;
	framebuffer.SetTargetWidthAndHeight(desc.Width,desc.Height);

	if(simulCloudRenderer)
	{
		B_RETURN(simulCloudRenderer->RestoreDeviceObjects(m_pd3dDevice));
		simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
	}
/*	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
	}*/
	if(simulSkyRenderer)
	{
		B_RETURN(simulSkyRenderer->RestoreDeviceObjects(m_pd3dDevice));
	//	if(simulCloudRenderer)
//			simulSkyRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
	}
	//if(simulAtmosphericsRenderer&&simulSkyRenderer)
	//	simulAtmosphericsRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
	//V_RETURN(CreateBuffers());

	/*if(simul2DCloudRenderer)
		V_RETURN(simul2DCloudRenderer->RestoreDeviceObjects(m_pd3dDevice));
	if(simulPrecipitationRenderer)
		V_RETURN(simulPrecipitationRenderer->RestoreDeviceObjects(m_pd3dDevice));
	*/
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->RestoreDeviceObjects(m_pd3dDevice);

	MakeCubeMatrices(view_matrices);
	return (hr==S_OK);
}

bool SimulWeatherRendererDX1x::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	framebuffer.InvalidateDeviceObjects();
	framebuffer_cubemap.InvalidateDeviceObjects();
	if(simulSkyRenderer)
		simulSkyRenderer->InvalidateDeviceObjects();
	if(simulCloudRenderer)
		simulCloudRenderer->InvalidateDeviceObjects();
	/*if(simul2DCloudRenderer)
		simul2DCloudRenderer->InvalidateDeviceObjects();
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->InvalidateDeviceObjects();
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->InvalidateDeviceObjects();*/
//	if(m_pTonemapEffect)
//        hr=m_pTonemapEffect->OnLostDevice();
      
// Free the cubemap resources. 
	SAFE_RELEASE(m_pImmediateContext);
	return (hr==S_OK);
}

bool SimulWeatherRendererDX1x::Destroy()
{
	HRESULT hr=S_OK;
	InvalidateDeviceObjects();
	if(simulSkyRenderer.get())
		simulSkyRenderer->Destroy();
	if(simulCloudRenderer.get())
		simulCloudRenderer->Destroy();
	Group::RemoveChild(simulSkyRenderer.get());
	simulSkyRenderer=NULL;
	Group::RemoveChild(simulCloudRenderer.get());
	simulCloudRenderer=NULL;
	/*if(simul2DCloudRenderer)
		simul2DCloudRenderer->Destroy();
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->Destroy();
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->InvalidateDeviceObjects();*/
	return (hr==S_OK);
}

SimulWeatherRendererDX1x::~SimulWeatherRendererDX1x()
{
	Destroy();
/*	SAFE_DELETE(simul2DCloudRenderer);
	SAFE_DELETE(simulPrecipitationRenderer);
	SAFE_DELETE(simulAtmosphericsRenderer);*/
}

/*bool SimulWeatherRendererDX1x::IsDepthFormatOk(D3DFORMAT DepthFormat, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat)
{
	LPDIRECT3D9 d3d;
	m_pd3dDevice->GetDirect3D(&d3d);
    // Verify that the depth format exists
    HRESULT hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat,
                                                       D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, DepthFormat);
    if(FAILED(hr))
		return (hr==S_OK);

    // Verify that the backbuffer format is valid
    hr=d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, D3DUSAGE_RENDERTARGET,
                                               D3DRTYPE_SURFACE, BackBufferFormat);
    if(FAILED(hr))
		return (hr==S_OK);

    // Verify that the depth format is compatible
    hr = d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, AdapterFormat, BackBufferFormat,
                                                    DepthFormat);

    return (hr==S_OK);
}
*/

bool SimulWeatherRendererDX1x::RenderCubemap()
{
	D3DXMATRIX ov=view;
	D3DXMATRIX op=proj;
	for(int i=0;i<6;i++)
	{
		framebuffer_cubemap.SetCurrentFace(i);
		framebuffer_cubemap.Activate();
		if(simulSkyRenderer)
		{
			D3DXMATRIX cube_proj;
			D3DXMatrixPerspectiveFovLH(&cube_proj,
				3.1415926536f/2.f,
				1.f,
				1.f,
				200000.f);
			view=view_matrices[i];
			proj=cube_proj;
			HRESULT hr=RenderSky(false,true);
		}
		framebuffer_cubemap.Deactivate();
	}
	view=ov;
	proj=op;
	return true;
}

void *SimulWeatherRendererDX1x::GetCubemap()
{
	return framebuffer_cubemap.GetTextureResource();// m_pCubeEnvMapSRV;
}

bool SimulWeatherRendererDX1x::RenderSky(bool buffered,bool is_cubemap)
{
	if(buffered)
	{
		framebuffer.Activate();
	}
	if(simulSkyRenderer)
		simulSkyRenderer->SetMatrices(view,proj);
	if(simulCloudRenderer)
		simulCloudRenderer->SetMatrices(view,proj);
/*	if(simul2DCloudRenderer)
		simul2DCloudRenderer->SetMatrices(view,proj);
	if(simulPrecipitationRenderer)
		simulPrecipitationRenderer->SetMatrices(view,proj);
	if(simulAtmosphericsRenderer)
		simulAtmosphericsRenderer->SetMatrices(view,proj);*/
	
	if(simulSkyRenderer)
	{
		float cloud_occlusion=0;
		if(show_3d_clouds&&simulCloudRenderer)
		{
			cloud_occlusion=simulCloudRenderer->GetSunOcclusion();
		}
		simulSkyRenderer->CalcSunOcclusion(cloud_occlusion);
	}
	HRESULT hr=S_OK;
	if(simulSkyRenderer)
		hr=simulSkyRenderer->Render(!buffered);
	// Do this AFTER sky render, to catch any changes to texture definitions:
	UpdateSkyAndCloudHookup();
	if(simulCloudRenderer)
		hr=simulCloudRenderer->Render(is_cubemap,false,UseDefaultFog);
	if(buffered)
		framebuffer.DeactivateAndRender(false);
	return (hr==S_OK);
}

void SimulWeatherRendererDX1x::Enable(bool sky,bool clouds3d,bool clouds2d,bool rain)
{
	show_3d_clouds=clouds3d;
	layer2=clouds2d;
	show_sky=sky;
	show_rain=rain;
}
void SimulWeatherRendererDX1x::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}

void SimulWeatherRendererDX1x::UpdateSkyAndCloudHookup()
{
	if(!simulSkyRenderer)
		return;
	void *l=0,*i=0;
	simulSkyRenderer->Get2DLossAndInscatterTextures(&l,&i);
	if(simulCloudRenderer)
	{
		simulCloudRenderer->SetLossTextures(l);
		simulCloudRenderer->SetInscatterTextures(i);
	}
}
void SimulWeatherRendererDX1x::Update(float dt)
{
	BaseWeatherRenderer::Update(dt);
	static bool pause=false;
    if(!pause)
	{
		UpdateSkyAndCloudHookup();
		if(simulSkyRenderer)
		{
			if(simulCloudRenderer)
			{
				/*simulCloudRenderer->SetLossTextures(simulSkyRenderer->GetLossTexture1(),
					simulSkyRenderer->GetLossTexture2());
				simulCloudRenderer->SetInscatterTextures(simulSkyRenderer->GetInscatterTexture1(),
					simulSkyRenderer->GetInscatterTexture2());*/
				simulCloudRenderer->SetFadeInterpolation(simulSkyRenderer->GetFadeInterp());
				simulCloudRenderer->SetAltitudeTextureCoordinate(simulSkyRenderer->GetAltitudeTextureCoordinate());
			}
		/*	if(simulAtmosphericsRenderer)
			{
				if(simulCloudRenderer)
					simulAtmosphericsRenderer->SetOvercastFactor(simulCloudRenderer->GetOvercastFactor());
				simulAtmosphericsRenderer->SetLossTextures(simulSkyRenderer->GetLossTexture1(),
					simulSkyRenderer->GetLossTexture2());
				simulAtmosphericsRenderer->SetInscatterTextures(simulSkyRenderer->GetInscatterTexture1(),
					simulSkyRenderer->GetInscatterTexture2());
				simulAtmosphericsRenderer->SetFadeInterpolation(simulSkyRenderer->GetFadeInterp());
			}*/
		}
		if(simulCloudRenderer)
		{
			simulCloudRenderer->Update(dt);
		}
/*		if(simul2DCloudRenderer)
			simul2DCloudRenderer->Update(dt);
		if(simulPrecipitationRenderer)
		{
			simulPrecipitationRenderer->Update(dt);
			if(simulCloudRenderer)
				simulPrecipitationRenderer->SetIntensity(simulCloudRenderer->GetPrecipitationIntensity());
			if(simulSkyRenderer)
			{
				simul::sky::float4 l=simulSkyRenderer->GetSkyInterface()->GetSunlightColour(0);
				simulPrecipitationRenderer->SetLightColour((const float*)(l));
			}
		}*/
	}
}

SimulSkyRendererDX1x *SimulWeatherRendererDX1x::GetSkyRenderer()
{
	return simulSkyRenderer.get();
}

SimulCloudRendererDX1x *SimulWeatherRendererDX1x::GetCloudRenderer()
{
	return simulCloudRenderer.get();
}

Simul2DCloudRenderer *SimulWeatherRendererDX1x::Get2DCloudRenderer()
{
	return NULL;
}

const char *SimulWeatherRendererDX1x::GetDebugText() const
{
	static char debug_text[256];
	if(simulSkyRenderer)
		sprintf_s(debug_text,256,"%3.3g",simulSkyRenderer->GetSkyKeyframer()->GetInterpolation());
//		sprintf_s(debug_text,256,"%s",simulCloudRenderer->GetDebugText());
//	if(simulCloudRenderer)
		//sprintf_s(debug_text,256,"TIME %2.2g ms\n%s",timing,simulCloudRenderer->GetDebugText());
//		sprintf_s(debug_text,256,"%s",simulCloudRenderer->GetDebugText());
	return debug_text;
}

float SimulWeatherRendererDX1x::GetTiming() const
{
	return timing;
}

//! Set a callback to fill in the depth/Z buffer in the lo-res sky texture.
void SimulWeatherRendererDX1x::SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb)
{
}