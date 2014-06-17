#pragma once
#include "Simul/Sky/BaseGpuSkyGenerator.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"

namespace simul
{
	namespace opengl
	{
		class GpuSkyGenerator:public simul::sky::BaseGpuSkyGenerator
		{
		public:
			GpuSkyGenerator();
			virtual ~GpuSkyGenerator();
			void RestoreDeviceObjects(crossplatform::RenderPlatform *);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			//! Return true if the derived class can make sky tables using the GPU.
			bool CanPerformGPUGeneration() const;
			void MakeLossAndInscatterTextures(int cycled_index,
				simul::sky::AtmosphericScatteringInterface *skyInterface
				,const simul::sky::GpuSkyParameters &gpuSkyParameters
				,const simul::sky::GpuSkyAtmosphereParameters &gpuSkyAtmosphereParameters
				,const simul::sky::GpuSkyInfraredParameters &gpuSkyInfraredParameters);
			virtual void CopyToMemory(int cycled_index,simul::sky::float4 *loss,simul::sky::float4 *insc,simul::sky::float4 *skyl);
			// If we want the generator to put the data directly into 3d textures:
			void SetDirectTargets(Texture **loss,Texture **insc,Texture **skyl,Texture *light_table)
			{
				for(int i=0;i<3;i++)
				{
					if(loss)
						finalLoss[i]=loss[i];
					else
						finalLoss[i]=NULL;
					if(insc)
						finalInsc[i]=insc[i];
					else
						finalInsc[i]=NULL;
					if(skyl)
						finalSkyl[i]=skyl[i];
					else
						finalSkyl[i]=NULL;
					this->light_table=light_table;
				}
			}
		protected:
		// framebuffer to render out by distance.
			FramebufferGL		fb[2];
			GLuint				loss_program;
			GLuint				insc_program;
			GLuint				skyl_program;
			GLuint				copy_program;
			simul::opengl::ConstantBuffer<GpuSkyConstants> gpuSkyConstants;
			Texture		*finalLoss[3];
			Texture		*finalInsc[3];
			Texture		*finalSkyl[3];
			Texture		*light_table;
			Texture		dens_tex,optd_tex;
			simul::sky::float4	*loss_cache;
			simul::sky::float4	*insc_cache;
			simul::sky::float4	*skyl_cache;
			int					cache_size;
		};
	}
}