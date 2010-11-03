// Copyright (c) 2007-2010 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#ifdef XBOX
	#include <xtl.h>
#else
	#include <d3d9.h>
	#include <d3dx9.h>
#endif
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Clouds/BaseWeatherRenderer.h"
class RenderDepthBufferCallback
{
public:
	virtual void Render()=0;
};
//! A rendering class that encapsulates Simul skies and clouds. Create an instance of this class within a DirectX program.
//! You can take this entire class and use it as source in your project.
//! Make appropriate modifications where required.
class SimulWeatherRenderer:public simul::clouds::BaseWeatherRenderer, public simul::graph::meta::Group
{
public:
	SimulWeatherRenderer(bool usebuffer=true,bool tonemap=false,int width=320,
		int height=240,bool sky=true,bool clouds3d=true,bool clouds2d=true,
		bool rain=true,
		bool colour_sky=false);
	virtual ~SimulWeatherRenderer();
	//standard d3d object interface functions
	HRESULT Create( LPDIRECT3DDEVICE9 pd3dDevice);
	//! Call this when the device has been created
	HRESULT RestoreDeviceObjects( LPDIRECT3DDEVICE9 pd3dDevice);
	//! Call this when the 3D device has been lost.
	HRESULT InvalidateDeviceObjects();
	//! Call this to release the memory for D3D device objects.
	HRESULT Destroy();
	//! Call this to draw the sky and clouds.
	HRESULT Render(bool is_cubemap=false);
	//! Call this to draw the clouds after the main scene.
	HRESULT RenderLateCloudLayer();
	//! Call this to draw lightning.
	HRESULT RenderLightning();
	//! Call this to draw rain etc.
	HRESULT RenderPrecipitation();
	//! Call this to draw sun flares etc. Draw this after geometry as it's an optical effect in the camera.
	HRESULT RenderFlares();
	//! Perform the once-per-frame time update.
	void Update(float dt);
	//! Apply the world, view and projection matrices, once per frame.
	void SetMatrices(const D3DXMATRIX &view,const D3DXMATRIX &proj);
	//! Set the exposure, if we're using an hdr shader to render the sky buffer.
	void SetExposure(float ex){exposure=ex;}
	//! Get a pointer to the sky renderer owned by this class instance.
	class SimulSkyRenderer *GetSkyRenderer();
	//! Get a pointer to the 3d cloud renderer owned by this class instance.
	class SimulCloudRenderer *GetCloudRenderer();
	//! Get a pointer to the 2d cloud renderer owned by this class instance.
	class Simul2DCloudRenderer *Get2DCloudRenderer();
	//! Get a pointer to the rain renderer owned by this class instance.
	class SimulPrecipitationRenderer *GetPrecipitationRenderer();
	//! Get a pointer to the atmospherics renderer owned by this class instance.
	class SimulAtmosphericsRenderer *GetAtmosphericsRenderer();
	//! Get the current debug text as a c-string pointer.
	const TCHAR *GetDebugText() const;
	//! Get a timing value - useful for performance evaluation.
	float GetTiming() const;

	//! Set a callback to fill in the depth/Z buffer in the lo-res sky texture.
	void SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb);
	void SetBufferSize(int w,int h);
	void EnableRain(bool e=true);
	float GetTotalBrightness() const;

	//! Connect-up sky, clouds:
	void ConnectInterfaces();
protected:
	void UpdateSkyAndCloudHookup();
	bool AlwaysRenderCloudsLate;
	bool RenderCloudsLate;
	//! The size of the 2D buffer the sky is rendered to.
	int BufferWidth,BufferHeight;
	LPDIRECT3DDEVICE9				m_pd3dDevice;
	LPDIRECT3DVERTEXDECLARATION9	m_pBufferVertexDecl;
	LPDIRECT3DTEXTURE9 temp_depth_texture;
	//! The HDR tonemapping hlsl effect used to render the hdr buffer to an ldr screen.
	LPD3DXEFFECT					m_pTonemapEffect;
	D3DXHANDLE						GammaTechnique;
	D3DXHANDLE						DirectTechnique;
	D3DXHANDLE						SkyToScreenTechnique;
	D3DXHANDLE						CloudBlendTechnique;
	D3DXHANDLE						m_hExposure;
	D3DXHANDLE						m_hGamma;
	D3DXHANDLE						bufferTexture;

	//! The texture the sky and clouds are rendered to.
	LPDIRECT3DTEXTURE9				hdr_buffer_texture;
	LPDIRECT3DTEXTURE9				buffer_depth_texture;
	//! The low dynamic range buffer the sky is rendered to.
	LPDIRECT3DTEXTURE9				ldr_buffer_texture;
	LPDIRECT3DSURFACE9				m_pHDRRenderTarget	;
	LPDIRECT3DSURFACE9				m_pBufferDepthSurface;
	LPDIRECT3DSURFACE9				m_pLDRRenderTarget	;
	HRESULT IsDepthFormatOk(D3DFORMAT DepthFormat, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat);
	HRESULT CreateBuffers();
	HRESULT RenderBufferToScreen(LPDIRECT3DTEXTURE9 texture,int w,int h,bool use_shader,bool blend=false);
	RenderDepthBufferCallback *renderDepthBufferCallback;
	simul::base::SmartPtr<class SimulSkyRenderer> simulSkyRenderer;
	simul::base::SmartPtr<class SimulCloudRenderer> simulCloudRenderer;
	simul::base::SmartPtr<class Simul2DCloudRenderer> simul2DCloudRenderer;
	simul::base::SmartPtr<class SimulPrecipitationRenderer> simulPrecipitationRenderer;
	simul::base::SmartPtr<class SimulAtmosphericsRenderer> simulAtmosphericsRenderer;
	float							exposure;
	float							gamma;
	LPDIRECT3DSURFACE9 MakeRenderTarget(const LPDIRECT3DTEXTURE9 pTexture);
	bool use_buffer;
	bool tone_map;
	float timing;
	float exposure_multiplier;
	bool show_rain;
};