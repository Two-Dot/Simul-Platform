// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Graph/Meta/Group.h"

namespace simul
{
	namespace opengl
	{
		class Effect;
		SIMUL_OPENGL_EXPORT_CLASS SimulGLHDRRenderer:public simul::graph::meta::Group
		{
		public:
			SimulGLHDRRenderer(int w,int h);
			~SimulGLHDRRenderer();
			void RecompileShaders();
			META_BeginProperties
				META_ValueProperty(float,Gamma,"")
				META_ValueProperty(float,Exposure,"")
			META_EndProperties
			void SetBufferSize(int w,int h);
			void RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform);
			void InvalidateDeviceObjects();
			bool StartRender(crossplatform::DeviceContext &deviceContext);
			bool FinishRender(crossplatform::DeviceContext &deviceContext);
			void RenderGlowTexture(crossplatform::DeviceContext &deviceContext);
			FramebufferGL framebuffer;
		protected:
			crossplatform::RenderPlatform *renderPlatform;
			FramebufferGL glow_fb;
			FramebufferGL alt_fb;
			bool initialized;
			// shaders
			Effect *effect;
			GLuint tonemap_program;
			GLint exposure_param;
			GLint gamma_param;
			GLint buffer_tex_param;
			GLuint glow_program;
			GLuint blur_program;
			float exposure, gamma;
		};
	}
}