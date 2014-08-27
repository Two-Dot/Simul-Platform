// Copyright (c) 2007-2008 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once

#include "Simul/Sky/BaseSkyRenderer.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/OpenGL/GpuSkyGenerator.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include <cstdlib>

namespace simul
{
	namespace sky
	{
		class SiderealSkyInterface;
		class AtmosphericScatteringInterface;
		class Sky;
		class FadeTableInterface;
		class SkyKeyframer;
		class OvercastCallback;
	}
	namespace opengl
	{
		//! A sky rendering class for OpenGL.
		SIMUL_OPENGL_EXPORT_CLASS SimulGLSkyRenderer : public simul::sky::BaseSkyRenderer
		{
		public:
			SimulGLSkyRenderer(simul::sky::SkyKeyframer *sk,simul::base::MemoryInterface *m);
			virtual ~SimulGLSkyRenderer();
			//standard ogl object interface functions

			//! Create the API-specific objects to be used in rendering. This is usually called from the SimulGLWeatherRenderer that
			//! owns this object.
			void						RestoreDeviceObjects(simul::crossplatform::RenderPlatform *);
			//! Destroy the API-specific objects used in rendering.
			void						InvalidateDeviceObjects();
			void						ReloadTextures();
			void						RecompileShaders();
			//! GL Implementation of render function.
			bool						Render(void *,bool blend);
			//! Draw the 2D fades to screen for debugging.
			bool						RenderFades(crossplatform::DeviceContext &deviceContext,int x,int y,int w,int h);
			// Implementing simul::sky::SkyTexturesCallback
			virtual void SetSkyTextureSize(unsigned ){}
			virtual void SetFadeTextureSize(unsigned ,unsigned ,unsigned ){}
			virtual void FillFadeTexturesSequentially(int ,int ,const float *,const float *)
			{
				std::cerr<<"exit(1)"<<std::endl;
				exit(1);
			}
			virtual		void CycleTexturesForward(){}
			virtual		bool HasFastFadeLookup() const{return true;}
			virtual		const float *GetFastLossLookup(crossplatform::DeviceContext &deviceContext,float distance_texcoord,float elevation_texcoord);
			virtual		const float *GetFastInscatterLookup(crossplatform::DeviceContext &deviceContext,float distance_texcoord,float elevation_texcoord);

			const		char *GetDebugText();
			simul::sky::BaseGpuSkyGenerator *GetBaseGpuSkyGenerator(){return &gpuSkyGenerator;}
		protected:
			simul::opengl::GpuSkyGenerator	gpuSkyGenerator;
			//! \internal Switch the current program, either sky_program or earthshadow_program.
			//! Also sets the parameter variables.	
			void		UseProgram(GLuint);
			void		SetFadeTexSize(int width_num_distances,int height_num_elevations,int num_altitudes);
			void		FillFadeTextureBlocks(int texture_index,int x,int y,int z,int w,int l,int d
						,const float *loss_float4_array,const float *inscatter_float4_array,const float *skylight_float4_array);

			void		EnsureTexturesAreUpToDate(void *);
			void		EnsureTextureCycle();

			bool		initialized;
			bool		Render2DFades(simul::crossplatform::DeviceContext &deviceContext);
			void		CreateSkyTextures();

			simul::opengl::Texture	light_table;

			bool					CreateSkyEffect();
			bool					RenderSkyToBuffer();

			unsigned				cloud_texel_index;
			unsigned char			*sky_tex_data;
	

			GLuint					fade_3d_to_2d_program;
			GLint					planetTexture_param;
			GLint					planetLightDir_param;
			GLint					planetColour_param;
		};
	}
}

