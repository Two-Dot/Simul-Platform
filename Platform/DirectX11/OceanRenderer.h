#ifndef SIMUL_OCEAN_RENDERER_DX1X_H
#define SIMUL_OCEAN_RENDERER_DX1X_H

#include "OceanSimulator.h"
#include "Simul/Platform/DirectX11/Export.h"
#include "Simul/Platform/DirectX11/Effect.h"
#include "Simul/Platform/CrossPlatform/Buffer.h"
#include "Simul/Terrain/BaseSeaRenderer.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "D3dx11effect.h"

#pragma warning(push)
#pragma warning(disable:4251)
namespace simul
{
	namespace dx11
	{
		SIMUL_DIRECTX11_EXPORT_CLASS OceanRenderer : public simul::terrain::BaseSeaRenderer
		{
		public:
			OceanRenderer(simul::terrain::SeaKeyframer *s);
			virtual ~OceanRenderer();
			// init & cleanup
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			// Rendering routines
			//! Call this once per frame to set the matrices.
			void SetMatrices(const float *v,const float *p);
			void Render(crossplatform::DeviceContext &context,float exposure);
			void RenderWireframe(crossplatform::DeviceContext &deviceContext);
			void Update(float dt);
			void SetCubemapTexture(crossplatform::Texture *c);
			void SetLossAndInscatterTextures(crossplatform::Texture *l,crossplatform::Texture *i,crossplatform::Texture *s);
			void RenderTextures(crossplatform::DeviceContext &deviceContext,int width,int depth);
		protected:
			OceanSimulator				*oceanSimulator;
			// HLSL shaders
			crossplatform::Effect		*effect;
			// D3D11 buffers and layout
			crossplatform::Buffer		*vertexBuffer;
			crossplatform::Buffer		*indexBuffer;
			crossplatform::Layout		*layout;

			// Color look up 1D texture
			crossplatform::Texture		*fresnelMap;

			// Distant perlin wave
			crossplatform::Texture		*perlinNoise;

			// Environment maps
			crossplatform::Texture		*cubemapTexture;
			// Atmospheric scattering
			crossplatform::Texture		*skyLossTexture;
			crossplatform::Texture		*skyInscatterTexture;
			crossplatform::Texture		*skylightTexture;
			crossplatform::ConstantBuffer<cbShading>		shadingConstants;
			crossplatform::ConstantBuffer<cbChangePerCall>	changePerCallConstants;
	
			// create a triangle strip mesh for ocean surface.
			void createSurfaceMesh();
			// create color/fresnel lookup table.
			void createFresnelMap(crossplatform::DeviceContext &deviceContext);
			// create perlin noise texture for far-sight rendering
			void loadTextures();
			void EnumeratePatterns(unsigned long* index_array);
		};
	}
}
#pragma warning(pop)
#endif