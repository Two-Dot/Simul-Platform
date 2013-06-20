// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Base/SmartPtr.h"
#include "Simul/Graph/Meta/Group.h"
#include "Simul/Clouds/BaseWeatherRenderer.h"
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
typedef unsigned GLuint;
class SimulGLSkyRenderer;
class SimulGLLightningRenderer;
namespace simul
{
	namespace clouds
	{
		class Environment;
	}
}
#ifndef RENDERDEPTHBUFFERCALLBACK
#define RENDERDEPTHBUFFERCALLBACK
class RenderDepthBufferCallback
{
public:
	virtual void Render()=0;
};
#endif
//! A rendering class that encapsulates Simul skies and clouds. Create an instance of this class within a DirectX program.
//! You can take this entire class and use it as source in your project.
//! Make appropriate modifications where required.
SIMUL_OPENGL_EXPORT_CLASS SimulGLWeatherRenderer:public simul::clouds::BaseWeatherRenderer
{
public:
	SimulGLWeatherRenderer(simul::clouds::Environment *env,bool usebuffer=true,bool tonemap=false,int width=640,
		int height=480,bool sky=true,
		bool rain=true);
	virtual ~SimulGLWeatherRenderer();
	void SetScreenSize(int w,int h);
	//! Call this when the device has been created
	void RestoreDeviceObjects(void*);
	void ReloadTextures();
	void RecompileShaders();
	//! Call this when the 3D device has been lost.
	void InvalidateDeviceObjects();
	//! Platform-dependent. Call this to draw the sky
	bool RenderSky(void *,float exposure,bool buffered,bool is_cubemap);
	void RenderSkyAsOverlay(void *context,float exposure,bool buffered,bool is_cubemap,const void* depthTexture);
	//! Call this to draw the clouds
	void RenderLateCloudLayer(void *context,float exposure,bool buf);
	//! Call this to draw lightning.
	void RenderLightning(void *context);
	//! Call this to draw rain etc.
	void RenderPrecipitation(void *context);
	//! Get a pointer to the sky renderer owned by this class instance.
	class SimulGLSkyRenderer *GetSkyRenderer();
	//! Get a pointer to the 3d cloud renderer owned by this class instance.
	class SimulGLCloudRenderer *GetCloudRenderer();
	//! Get a pointer to the 2d cloud renderer owned by this class instance.
	class SimulGL2DCloudRenderer *Get2DCloudRenderer();
	//! Get a pointer to the rain renderer owned by this class instance.
	//class SimulGLPrecipitationRenderer *GetPrecipitationRenderer();
	//! Get a pointer to the atmospherics renderer owned by this class instance.
	//class SimulGLAtmosphericsRenderer *GetAtmosphericsRenderer();
	//! Set a callback to fill in the depth/Z buffer in the lo-res sky texture.
	void SetRenderDepthBufferCallback(RenderDepthBufferCallback *cb);
	void EnableRain(bool e=true);
	void EnableCloudLayers();
//	const char *GetDebugText() const;
	GLuint GetFramebufferTexture();
protected:
	std::string shader;
	//! This is set once the GL device has been initialized - then we can create textures and so forth.
	bool					device_initialized;
	class	FramebufferGL	*scene_buffer;
	bool					externally_defined_buffers;
	bool					auto_exposure;
	int						BufferWidth,BufferHeight;	//< The size of the 2D buffer the sky is rendered to.
	float					exposure;
	float					gamma;
	bool					use_buffer;
	bool					tone_map;
	GLuint					cloud_overlay_program;
	RenderDepthBufferCallback *renderDepthBufferCallback;
	simul::base::SmartPtr<class SimulGLSkyRenderer> simulSkyRenderer;
	simul::base::SmartPtr<class SimulGLCloudRenderer> simulCloudRenderer;
	simul::base::SmartPtr<class SimulGL2DCloudRenderer> simul2DCloudRenderer;
	simul::base::SmartPtr<class SimulGLLightningRenderer> simulLightningRenderer;
	simul::base::SmartPtr<class SimulGLPrecipitationRenderer> simulPrecipitationRenderer;
	simul::base::SmartPtr<class SimulGLAtmosphericsRenderer> simulAtmosphericsRenderer;
	void CreateBuffers();
	void RenderBufferToScreen(GLuint texture,int w,int h,bool use_shader,bool blend=false);
	virtual void DrawLines(VertexXyzRgba *vertices,int vertex_count,bool strip){}
};
#ifdef _MSC_VER
	#pragma warning(pop)
#endif